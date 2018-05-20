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

	// Initialize regions
	for (int i = 0; i < CB_NUM_REGIONS; i++) {
		regions[i].data = NULL;
		regions[i].data_size = 0;
		r = pthread_rwlock_init(&regions[i].rwlock, NULL);
		if (r != 0) cb_eperror(r);
	}
}

void free_globals(void)
{
	free(child_conn_list);
	free(app_conn_list);

	pthread_rwlock_destroy(&child_conn_list_rwlock);
	pthread_rwlock_destroy(&app_conn_list_rwlock);
	pthread_rwlock_destroy(&mode_rwlock);

	for (int i = 0; i < CB_NUM_REGIONS; i++) {
		free(regions[i].data);
		pthread_rwlock_destroy(&regions[i].rwlock);
	}
}

void create_threads(char *argv[], pthread_t *parent_serve_tid, pthread_t *app_accept_tid, pthread_t *child_accept_tid)
{
	int r;

	// Create thread for parent connection (if needed)
	if (!root) {
		r = pthread_create(parent_serve_tid, NULL, serve_parent, argv);
		if (r != 0) cb_eperror(r);
	}
	// Create listening threads
	r = pthread_create(app_accept_tid, NULL, app_accept, NULL);
	if (r != 0) cb_eperror(r);
	r = pthread_create(child_accept_tid, NULL, child_accept, NULL);
	if (r != 0) cb_eperror(r);
}

void main_cleanup(pthread_t parent_serve_tid, pthread_t app_accept_tid, pthread_t child_accept_tid)
{
	int r;

	// Cancel and join threads
	if (!root) {
		r = pthread_cancel(parent_serve_tid);
		if (r != 0) cb_eperror(r);
	}
	r = pthread_cancel(app_accept_tid);
	if (r != 0) cb_eperror(r);
	r = pthread_cancel(child_accept_tid);
	if (r != 0) cb_eperror(r);

	void *ret; // TODO: remove assert and ret
	if (!root) {
		r = pthread_join(parent_serve_tid, &ret);
		if (r != 0) cb_eperror(r);
		assert(ret == PTHREAD_CANCELED);
	}
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
	/* In order to launch the clipboard in connected mode it is necessary for
	 * the user to use the command line argument -c followed by the
	 * address and port of another clipboard. */
	if (argc > 1) {
		if (argc != 4) {
			printf("Usage: %s -c <address> <port>\n", argv[0]);
			exit(EXIT_FAILURE);
		}

		root = false;
	}

	int r;

	// Block most signals. All threads will inherit the signal mask,
	// and hence will also block most signals.
	sigset_t sigset;
	r = block_signals(&sigset);
	if (r == -1) cb_eperror(r);

	// Initialize globals declared in clipboard.h
	init_globals();

	// Create parent, app & child listening threads
	pthread_t parent_serve_tid, app_accept_tid, child_accept_tid;
	create_threads(argv, &parent_serve_tid, &app_accept_tid, &child_accept_tid);

	// Handle signals via sigwait() on main thread
	while (1) {
		int sig;

		r = sigwait(&sigset, &sig);
		if (r != 0) { cb_perror(r); sig = SIGTERM; }

		cb_log("%s: got signal %d\n", argv[0], sig);

		if (sig == SIGINT || sig == SIGTERM) {
			puts("Exiting...");
			main_cleanup(parent_serve_tid, app_accept_tid, child_accept_tid);
			pthread_exit(NULL);
		}
	}

	return EXIT_SUCCESS;
}
