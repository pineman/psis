#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#include "clipboard.h"
#include "local.h"
#include "remote.h"
#include "parent.h"

/* Globals */
// Global array of regions
struct region local_regions[CB_NUM_REGIONS];
// Global array of remote connections
struct conn remote_connections[CB_MAX_REMOTE_CONN];
// Global array of local connections
struct conn local_connections[CB_MAX_LOCAL_CONN];
// Are we in connected mode?
bool connected_mode = false;
pthread_mutex_t mode_lock = PTHREAD_MUTEX_INITIALIZER; // protects `connected_mode`

int main(int argc, char *argv[])
{
	int r;
	pthread_t local_accept_thread, remote_accept_thread, parent_serve_thread;

	/* In order to launch the clipboard in connected mode it is necessary for
	 * the user to use the command  line  argument  -c followed  by  the
	 * address  and  port  of  another  clipboard. */
	if (argc > 1) {
		if (argc != 4) {
			puts("-c <address> <port>");
			exit(EXIT_FAILURE);
		}

		connected_mode = true;
	}

	// Block all signals. All threads will inherit the signal mask,
	// and hence will also block all signals.
	sigset_t oldsig, allsig;
	r = sigfillset(&allsig);
	if (r != 0) emperror(errno);
	r = pthread_sigmask(SIG_SETMASK, &allsig, &oldsig);
	if (r != 0) emperror(r);

	pthread_attr_t attr;
	r = pthread_attr_init(&attr);
	if (r != 0) emperror(r);
	//pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	// Thread for parent connection
	r = pthread_create(&parent_serve_thread, &attr, serve_parent, argv);
	if (r != 0) emperror(r);
	// Create listening threads
	r = pthread_create(&local_accept_thread, &attr, local_accept_loop, NULL);
	if (r != 0) emperror(r);
	r = pthread_create(&remote_accept_thread, &attr, remote_accept_loop, NULL);
	if (r != 0) emperror(r);

	// Handle signals via sigwaitinfo() on main thread
	while (1) {
		int sig;
		siginfo_t siginfo;

		sig = sigwaitinfo(&allsig, &siginfo);
		if (sig == -1) emperror(errno);

		if (sig == SIGINT) {
			puts("Exiting...");
			// TODO: maybe cleanup function
			r = pthread_cancel(local_accept_thread);
			if (r != 0) emperror(r);
			r = pthread_cancel(remote_accept_thread);
			if (r != 0) emperror(r);
			r = pthread_cancel(parent_serve_thread);
			if (r != 0) emperror(r);

			void *ret;
			r = pthread_join(local_accept_thread, &ret);
			if (r != 0) emperror(r);
			assert(ret == PTHREAD_CANCELED);

			r = pthread_join(remote_accept_thread, &ret);
			if (r != 0) emperror(r);
			assert(ret == PTHREAD_CANCELED);

			r = pthread_join(parent_serve_thread, &ret);
			if (r != 0) emperror(r);
			assert(ret == PTHREAD_CANCELED);

			r = pthread_mutex_destroy(&mode_lock);
			if (r != 0) emperror(r);

			pthread_exit(NULL);
		}
	}

	return EXIT_SUCCESS;
}
