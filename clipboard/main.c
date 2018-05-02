#include <stdlib.h>

#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "cb_common.h"
#include "local.h"
#include "remote.h"

char *local_regions[CB_NUM_REGIONS];

int main(int argc, char *argv[])
{
	int r;

	/* In order to launch the clipboard in connected mode it is necessary for
	 * the user to use the command  line  argument  -c followed  by  the
	 * address  and  port  of  another  clipboard.
	 * Although a clipboard can work in single or connected mode, it should be
	 * able to always receive connections from the local applications or remote
	 * clipboards. => All clipboards listen on a random port
	 */
	if (argc > 1) {
		if (argc != 4) {
			puts("-c <address> <port>");
			exit(1);
		}
		// Connected mode
		printf("Connecting to %s:%s\n", argv[2], argv[3]);

		// TODO: Connect to a parent remote clipboard given in argv[]
	}

	// Block all signals. All threads will inherit the signal mask,
	// and hence will also block all signals.
	sigset_t oldsig, allsig;
	r = sigfillset(&allsig);
	if (r != 0) eperror(errno);
	r = pthread_sigmask(SIG_SETMASK, &allsig, &oldsig);
	if (r != 0) eperror(r);

	pthread_t lt, rt;
	r = pthread_create(&lt, NULL, local_accept_thread, NULL);
	if (r != 0) eperror(r);
	r = pthread_create(&rt, NULL, remote_accept_thread, NULL);
	if (r != 0) eperror(r);

	// Handle signals via sigwaitinfo() on main thread
	while (1) {
		int sig;
		siginfo_t siginfo;

		sig = sigwaitinfo(&allsig, &siginfo);
		if (sig == -1) eperror(errno);

		if (siginfo.si_signo == SIGINT) {
			puts("Got SIGINT!");
			exit(1);
		}
	}

	return EXIT_SUCCESS;
}
