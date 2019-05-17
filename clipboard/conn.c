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
		t = conn->tid; // TODO: race condition here with cleanup_serve_app -> conn_destroy -> free(conn)
		// With one server and one client app:
		/*
		WARNING: ThreadSanitizer: data race (pid=9329)
  Write of size 8 at 0x7b1400002800 by thread T3:
    #0 free <null> (clipboard+0x7015f)
    #1 conn_destroy /home/pineman/code/proj/psis/clipboard/conn.c:31:2 (clipboard+0xc5ba8)
    #2 cleanup_serve_app /home/pineman/code/proj/psis/clipboard/app.c:149:2 (clipboard+0xc7e8b)
    #3 serve_app /home/pineman/code/proj/psis/clipboard/app.c:87:2 (clipboard+0xc7ce9)
    #4 cb_recv /home/pineman/code/proj/psis/cb_common/cb_msg.c:86:11 (clipboard+0xc46cf)
    #5 cb_recv_msg /home/pineman/code/proj/psis/cb_common/cb_msg.c:161:6 (clipboard+0xc48ef)
    #6 serve_app /home/pineman/code/proj/psis/clipboard/app.c:91:7 (clipboard+0xc76cf)

  Previous read of size 8 at 0x7b1400002800 by thread T2:
    #0 conn_cancel_all /home/pineman/code/proj/psis/clipboard/conn.c:110:13 (clipboard+0xc61c6)
    #1 cleanup_app_accept /home/pineman/code/proj/psis/clipboard/app.c:66:6 (clipboard+0xc74dd)
    #2 app_accept /home/pineman/code/proj/psis/clipboard/app.c:49:2 (clipboard+0xc746c)
    #3 conn_accept_loop /home/pineman/code/proj/psis/clipboard/conn.c:57:16 (clipboard+0xc5f3b)
    #4 app_accept /home/pineman/code/proj/psis/clipboard/app.c:51:2 (clipboard+0xc7422)

  Thread T3 (tid=9351, running) created by thread T2 at:
    #0 pthread_create <null> (clipboard+0x54207)
    #1 conn_accept_loop /home/pineman/code/proj/psis/clipboard/conn.c:70:7 (clipboard+0xc5fb0)
    #2 app_accept /home/pineman/code/proj/psis/clipboard/app.c:51:2 (clipboard+0xc7422)

  Thread T2 (tid=9332, running) created by main thread at:
    #0 pthread_create <null> (clipboard+0x54207)
    #1 create_threads /home/pineman/code/proj/psis/clipboard/main.c:129:6 (clipboard+0xca9a2)
    #2 listen_sockets /home/pineman/code/proj/psis/clipboard/main.c:102:2 (clipboard+0xca534)
    #3 main /home/pineman/code/proj/psis/clipboard/main.c:191:2 (clipboard+0xcaee4)

SUMMARY: ThreadSanitizer: data race (/home/pineman/code/proj/psis/clipboard/clipboard+0x7015f) in __interceptor_free
*/
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
