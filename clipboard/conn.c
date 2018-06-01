#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>

#include "conn.h"
#include "clipboard.h"
#include "utils.h"

int conn_new(struct conn **conn, int sockfd)
{
	int r;

	*conn = calloc(1, sizeof(struct conn));
	if (*conn == NULL) return errno;

	r = init_mutex(&(*conn)->mutex);
	if (r != 0) return r;

	(*conn)->sockfd = sockfd;
	return 0;
}

void conn_destroy(struct conn *conn)
{
	(void) pthread_mutex_destroy(&conn->mutex);
	(void) close(conn->sockfd);
	free(conn);
}

// Append to a conn_list (unsafe, caller must lock the list)
void conn_append(struct conn *head, struct conn *new)
{
	assert(head != NULL);
	assert(head->prev == NULL);
	assert(new != NULL);

	// Prev (might be NULL if list is empty)
	struct conn *old = head->next;
	if (old != NULL) old->prev = new;
	new->next = old;
	// Next (is new)
	head->next = new;
	new->prev = head;
}

// This generic accept loop is used by the app accept thread and the children accept thread.
void conn_accept_loop(int server, struct conn *head, pthread_rwlock_t *rwlock, void *(*handle_conn)(void *))
{
	int r;

	while (1) {
		// Accept connections
		int client = accept(server, NULL, 0);
		if (client == -1) { r = errno; goto cont; }

		// Create a new connection struct and append to the list.
		// Disable thread cancellation to prevent unknown states in the list.
		(void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		// New connection
		struct conn *conn = NULL;
		r = conn_new(&conn, client);
		if (r != 0) goto cont_conn;

		// Create the connection handler thread
		r = pthread_create(&conn->tid, NULL, handle_conn, conn);
		if (r != 0) goto cont_conn;

		// Critical section: append to connection list.
		r = pthread_rwlock_wrlock(rwlock);
		if (r != 0) goto cont_conn;
		conn_append(head, conn);
		r = pthread_rwlock_unlock(rwlock);
		assert(r == 0);

		(void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		continue;

	// If an error occurs inside the accept loop, print it but otherwise keep
	// accepting connections.
	cont_conn:
		conn_destroy(conn);
	cont:
		cb_perror(r);
	}
}

int conn_cancel_all(struct conn *head, pthread_rwlock_t *rwlock)
{
	int r;
	struct conn *conn;
	pthread_t t;

	// Cancel all connection threads (always fetch next of dummy head)
	while (1) {
		// Critical section: reading from the list.
		r = pthread_rwlock_rdlock(rwlock);
		if (r != 0) return r;
		conn = head->next;
		r = pthread_rwlock_unlock(rwlock);
		if (r != 0) return r;
		if (conn == NULL) break;

		// Cancel and join thread (connection)
		t = conn->tid;
		r = pthread_cancel(t);
		if (r != 0) return r;
		r = pthread_join(t, NULL);
		if (r != 0) return r;
	}

	return 0;
}

// Safely remove from a conn_list
int conn_remove(struct conn *rm, pthread_rwlock_t *rwlock)
{
	assert(rm != NULL);
	int r;

	// Critical section: writing to a connection list.
	r = pthread_rwlock_wrlock(rwlock);
	if (r != 0) return r;

	if (rm->next) rm->next->prev = rm->prev;
	if (rm->prev) rm->prev->next = rm->next;

	r = pthread_rwlock_unlock(rwlock);
	if (r != 0) return r;

	return 0;
}
