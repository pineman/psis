#include <stdlib.h>
#include <stdbool.h>

#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#include "clipboard.h"
#include "local.h"
#include "remote.h"
#include "parent.h"

struct conn {
	int sockfd;
	pthread_mutex_t lock; // Protects writes to sockfd // TODO: can be rwlock?
	// TODO: lock per connection or globally per update?
	// TODO: pthread_t tid; // TO CANCEL THE THREAD
};

struct region {
	char data[CB_DATA_MAX_SIZE];
	pthread_mutex_t lock; // Protects `data`
};

// Global array of regions
struct region local_regions[CB_NUM_REGIONS];
// Are we the root node or not? (yes if we have no parent [single mode] or
// our connection to our parent died).
bool root = false;

int main(int argc, char *argv[])
{
	int r;
	pthread_t local_accept_thread, remote_accept_thread;

	/* In order to launch the clipboard in connected mode it is necessary for
	 * the user to use the command  line  argument  -c followed  by  the
	 * address  and  port  of  another  clipboard. */
	if (argc > 1) {
		if (argc != 4) {
			puts("-c <address> <port>");
			exit(EXIT_FAILURE);
		}

		// Connect to a parent remote clipboard given in argv[]
		int parent = connect_parent(argv[2], argv[3]);

		struct thread_args *targs = malloc(sizeof(struct thread_args));
		if (targs == NULL) emperror(errno);
		targs->client = parent;

		// Thread for parent connection
		r = pthread_create(&targs->thread_id, NULL, serve_parent, targs);
		if (r != 0) emperror(r);
	}

	// Block all signals. All threads will inherit the signal mask,
	// and hence will also block all signals.
	sigset_t oldsig, allsig;
	r = sigfillset(&allsig);
	if (r != 0) emperror(errno);
	r = pthread_sigmask(SIG_SETMASK, &allsig, &oldsig);
	if (r != 0) emperror(r);

	r = pthread_create(&local_accept_thread, NULL, local_accept_loop, NULL);
	if (r != 0) emperror(r);
	r = pthread_create(&remote_accept_thread, NULL, remote_accept_loop, NULL);
	if (r != 0) emperror(r);

	// Handle signals via sigwaitinfo() on main thread
	while (1) {
		int sig;
		siginfo_t siginfo;

		sig = sigwaitinfo(&allsig, &siginfo);
		if (sig == -1) emperror(errno);

		if (siginfo.si_signo == SIGINT) {
			puts("Got SIGINT!");
			// TODO: Close all connections and cancel all threads?
			// Maybe cancel all threads and they do their cleanup?
			exit(1);
		}
	}

	return EXIT_SUCCESS;
}
