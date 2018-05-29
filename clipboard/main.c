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
pthread_rwlock_t parent_conn_rwlock; // Protects parent_conn and root

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
	r = pthread_rwlock_init(&parent_conn_rwlock, NULL);
	if (r != 0) cb_eperror(r);

	init_regions();
}

void free_globals(void)
{
	free(child_conn_list);
	free(app_conn_list);

	pthread_rwlock_destroy(&child_conn_list_rwlock);
	pthread_rwlock_destroy(&app_conn_list_rwlock);
	pthread_rwlock_destroy(&parent_conn_rwlock);

	destroy_regions();
}

void create_threads(char *argv[])
{
	int r;

	int *parent_socket; 
	if (!root) {
		parent_socket = malloc(sizeof(int));
		*parent_socket = connect_parent(argv[2], argv[3]);
		if (*parent_socket == -1) {
			fprintf(stderr, "Error connecting to parent.\n");
			cb_perror(errno);
			exit(EXIT_FAILURE);
		}
	}
	int *children_socket = malloc(sizeof(int));
	*children_socket = listen_child();
	if (*children_socket == -1) {
		fprintf(stderr, "Error listening for child clipboards.\n");
		cb_perror(errno);
		exit(EXIT_FAILURE);
	}
	int *app_socket = malloc(sizeof(int));
	*app_socket = listen_local();
	if (*app_socket == -1) {
		fprintf(stderr, "Error listening for local applications.\n");
		cb_perror(errno);
		exit(EXIT_FAILURE);
	}

	// Create thread for parent connection (if needed)
	if (!root) {
		r = pthread_create(&parent_serve_tid, NULL, serve_parent, parent_socket);
		if (r != 0) cb_eperror(r);
	}
	// Create accept threads
	r = pthread_create(&child_accept_tid, NULL, child_accept, children_socket);
	if (r != 0) cb_eperror(r);

	r = pthread_create(&app_accept_tid, NULL, app_accept, app_socket);
	if (r != 0) cb_eperror(r);
}

void main_cleanup(void *arg)
{
	(void) arg;
	int r;

	cb_log("%s", "main cleanup\n");
	// Cancel and join threads
	if (!root) {
		r = pthread_cancel(parent_serve_tid);
		if (r != 0) cb_eperror(r);
		r = pthread_join(parent_serve_tid, NULL);
		if (r != 0) cb_eperror(r);
		cb_log("%s", "join parent\n");
	}

	r = pthread_cancel(app_accept_tid);
	if (r != 0) cb_eperror(r);
	r = pthread_join(app_accept_tid, NULL);
	if (r != 0) cb_eperror(r);
	cb_log("%s", "join app\n");

	r = pthread_cancel(child_accept_tid);
	if (r != 0) cb_eperror(r);
	r = pthread_join(child_accept_tid, NULL);
	if (r != 0) cb_eperror(r);
	cb_log("%s", "join child\n");

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

	// Block most signals. All threads will inherit the signal mask,
	// and hence will also block most signals.
	sigset_t sigset;
	r = block_signals(&sigset);
	if (r == -1) { cb_perror(r); exit(EXIT_FAILURE); }

	// Create parent, app & child listening threads
	create_threads(argv);

	pthread_cleanup_push(main_cleanup, NULL);

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
