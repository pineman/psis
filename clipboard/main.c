#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#include "clipboard.h"
#include "app.h"
#include "child.h"
#include "parent.h"

/* Globals */
// Global array of regions
struct region app_regions[CB_NUM_REGIONS];

// Global array of child connections // TODO: linked list
struct conn *child_conn;
// Global array of app connections // TODO: linked list
struct conn *app_conn;

struct conn *parent_conn;

// Are we in connected mode or are we the master (i.e. we have no parent)
bool master = true;
pthread_mutex_t mode_lock; // protects `master` boolean

int main(int argc, char *argv[])
{
	/* In order to launch the clipboard in connected mode it is necessary for
	 * the user to use the command  line  argument  -c followed  by  the
	 * address  and  port  of  another  clipboard. */
	if (argc > 1) {
		if (argc != 4) {
			puts("-c <address> <port>");
			exit(EXIT_FAILURE);
		}

		master = false;
	}

	int r;
	pthread_t app_accept_thread, child_accept_thread, parent_serve_thread;

	// Block all signals. All threads will inherit the signal mask,
	// and hence will also block all signals.
	sigset_t oldsig, allsig;
	r = sigfillset(&allsig);
	if (r != 0) cb_eperror(errno);
	r = pthread_sigmask(SIG_SETMASK, &allsig, &oldsig);
	if (r != 0) cb_eperror(r);

	pthread_mutexattr_t mutex_attr;
	r = pthread_mutexattr_init(&mutex_attr);
	if (r != 0) cb_eperror(r);
	r = pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
	if (r != 0) cb_eperror(r);
	r = pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST);
	if (r != 0) cb_eperror(r);
	r = pthread_mutex_init(&mode_lock, &mutex_attr);
	if (r != 0) cb_eperror(r);

	pthread_attr_t attr;
	r = pthread_attr_init(&attr);
	if (r != 0) cb_eperror(r);
	//pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	// Thread for parent connection
	if (!master) {
		r = pthread_create(&parent_serve_thread, &attr, serve_parent, argv);
		if (r != 0) cb_eperror(r);
	}
	// Create listening threads
	r = pthread_create(&app_accept_thread, &attr, app_accept, NULL);
	if (r != 0) cb_eperror(r);
	r = pthread_create(&child_accept_thread, &attr, child_accept, NULL);
	if (r != 0) cb_eperror(r);

	// Handle signals via sigwait() on main thread
	while (1) {
		int sig;

		r = sigwait(&allsig, &sig);
		if (r != 0) cb_eperror(r);

		printf("clipboard: got signal %d\n", sig);

		if (sig == SIGINT) {
			puts("Exiting...");
			// TODO: maybe cleanup function
			r = pthread_cancel(app_accept_thread);
			if (r != 0) cb_eperror(r);
			r = pthread_cancel(child_accept_thread);
			if (r != 0) cb_eperror(r);

			if (!master) {
				r = pthread_cancel(parent_serve_thread);
				if (r != 0) cb_eperror(r);
			}

			void *ret;
			r = pthread_join(app_accept_thread, &ret);
			if (r != 0) cb_eperror(r);
			assert(ret == PTHREAD_CANCELED);

			r = pthread_join(child_accept_thread, &ret);
			if (r != 0) cb_eperror(r);
			assert(ret == PTHREAD_CANCELED);

			if (!master) {
				r = pthread_join(parent_serve_thread, &ret);
				if (r != 0) cb_eperror(r);
				assert(ret == PTHREAD_CANCELED);
			}

			r = pthread_mutex_destroy(&mode_lock);
			if (r != 0) cb_eperror(r);

			pthread_exit(NULL);
		}
	}

	return EXIT_SUCCESS;
}
