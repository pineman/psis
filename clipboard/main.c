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

/* Globals */
pthread_t main_tid, parent_serve_tid, app_accept_tid, child_accept_tid;
// Global array of regions
struct region regions[CB_NUM_REGIONS];

// Connection lists
// Dummy head node doubly-linked list
struct conn *child_conn_list;
pthread_rwlock_t child_conn_list_rwlock; // Protects child_conn_list
struct conn *app_conn_list;
pthread_rwlock_t app_conn_list_rwlock; // Protects app_conn_list

// Parent connection
struct conn *parent_conn;
// Are we in connected mode or are we the root (i.e. we have no parent)
bool root;

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

	init_regions();
}

void free_globals(void)
{
	free(child_conn_list);
	free(app_conn_list);

	pthread_rwlock_destroy(&child_conn_list_rwlock);
	pthread_rwlock_destroy(&app_conn_list_rwlock);

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

	// Cancel and join threads
	if (!root) {
		r = pthread_cancel(parent_serve_tid);
		if (r != 0) cb_eperror(r);
		r = pthread_join(parent_serve_tid, NULL);
		if (r != 0) cb_eperror(r);
	}
	r = pthread_cancel(app_accept_tid);
	if (r != 0) cb_eperror(r);
	r = pthread_cancel(child_accept_tid);
	if (r != 0) cb_eperror(r);

	r = pthread_join(app_accept_tid, NULL);
	if (r != 0) cb_eperror(r);
	r = pthread_join(child_accept_tid, NULL);
	if (r != 0) cb_eperror(r);

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
