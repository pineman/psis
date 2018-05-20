#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#include "clipboard.h"
#include "utils.h"
#include "app.h"
#include "child.h"
#include "parent.h"

void init_globals(void)
{
	int r;

	main_tid = pthread_self();

	// Initialize conn_list dummy heads
	child_conn_list = calloc(1, sizeof(struct conn));
	if (child_conn_list == NULL) cb_eperror(errno);
	r = pthread_rwlock_init(&child_conn_list_rwlock, NULL);
	if (r != 0) cb_eperror(r);

	app_conn_list = calloc(1, sizeof(struct conn));
	if (app_conn_list == NULL) cb_eperror(errno);
	r = pthread_rwlock_init(&app_conn_list_rwlock, NULL);
	if (r != 0) cb_eperror(r);

	root = true;
	r = pthread_rwlock_init(&mode_rwlock, NULL);
	if (r != 0) cb_eperror(r);

	init_regions();
}

void free_globals(void)
{
	free(child_conn_list);
	free(app_conn_list);

	pthread_rwlock_destroy(&child_conn_list_rwlock);
	pthread_rwlock_destroy(&app_conn_list_rwlock);
	pthread_rwlock_destroy(&mode_rwlock);

	destroy_regions();
}

void create_threads(char *argv[])
{
	int r;

	// Create thread for parent connection (if needed)
	if (!root) {
		r = pthread_create(&parent_serve_tid, NULL, serve_parent, argv);
		if (r != 0) cb_eperror(r);
	}
	// Create listening threads
	r = pthread_create(&app_accept_tid, NULL, app_accept, NULL);
	if (r != 0) cb_eperror(r);
	r = pthread_create(&child_accept_tid, NULL, child_accept, NULL);
	if (r != 0) cb_eperror(r);
}

void main_cleanup(void *arg)
{
	(void) arg;
	int r;
	void *ret; // TODO: remove assert and ret

	// Cancel and join threads
	r = pthread_rwlock_rdlock(&mode_rwlock);
	if (r != 0) cb_eperror(r);
	bool mroot = root;
	r = pthread_rwlock_unlock(&mode_rwlock);
	if (r != 0) cb_eperror(r);
	if (!mroot) {
		r = pthread_cancel(parent_serve_tid);
		if (r != 0) cb_eperror(r);
		r = pthread_join(parent_serve_tid, &ret);
		if (r != 0) cb_eperror(r);
		assert(ret == PTHREAD_CANCELED);
	}
	r = pthread_cancel(app_accept_tid);
	if (r != 0) cb_eperror(r);
	r = pthread_cancel(child_accept_tid);
	if (r != 0) cb_eperror(r);

	r = pthread_join(app_accept_tid, &ret);
	if (r != 0) cb_eperror(r);
	assert(ret == PTHREAD_CANCELED);
	r = pthread_join(child_accept_tid, &ret);
	if (r != 0) cb_eperror(r);
	assert(ret == PTHREAD_CANCELED);

	free_globals();
}

int main(int argc, char *argv[])
{
	// Initialize globals declared in clipboard.h
	init_globals();

	/* In order to launch the clipboard in connected mode it is necessary for
	 * the user to use the command line argument -c followed by the
	 * address and port of another clipboard. */
	if (argc > 1) {
		if (argc != 4) {
			printf("Usage: %s -c <address> <port>\n", argv[0]);
			free_globals();
			exit(EXIT_FAILURE);
		}

		root = false;
	}

	int r;

	pthread_cleanup_push(main_cleanup, NULL);

	// Block most signals. All threads will inherit the signal mask,
	// and hence will also block most signals.
	sigset_t sigset;
	r = block_signals(&sigset);
	if (r == -1) cb_eperror(r);

	// Create parent, app & child listening threads
	create_threads(argv);

	// Handle signals via sigwait() on main thread
	while (1) {
		int sig;

		r = sigwait(&sigset, &sig);
		if (r != 0) { cb_perror(r); sig = SIGTERM; }

		cb_log("%s: got signal %d\n", argv[0], sig);

		if (sig == SIGINT || sig == SIGTERM) {
			puts("Exiting...");
			break;
		}
	}

	pthread_cleanup_pop(1);

	return EXIT_SUCCESS;
}
