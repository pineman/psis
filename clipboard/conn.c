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

// Safely append to a conn_list
int conn_append(struct conn *head, struct conn *new, pthread_rwlock_t *rwlock)
{
	// TODO: remove assert
	assert(head != NULL);
	assert(head->prev == NULL);
	assert(new != NULL);

	int r;

	r = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (r != 0) return r;
	r = pthread_rwlock_wrlock(rwlock);
	if (r != 0) return r;

	// Prev (might be NULL if list is empty)
	struct conn *old = head->next;
	if (old != NULL) old->prev = new;
	new->next = old;
	// Next (is new)
	head->next = new;
	new->prev = head;

	r = pthread_rwlock_unlock(rwlock);
	if (r != 0) return r;
	r = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if (r != 0) return r;

	return 0;
}

// Safely remove from a conn_list
int conn_remove(struct conn *rm, pthread_rwlock_t *rwlock)
{
	// TODO: remove assert
	assert(rm != NULL);

	int r;

	r = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (r != 0) return r;
	r = pthread_rwlock_wrlock(rwlock);
	if (r != 0) return r;

	if (rm->next) rm->next->prev = rm->prev;
	if (rm->prev) rm->prev->next = rm->next;

	r = pthread_rwlock_unlock(rwlock);
	if (r != 0) return r;
	r = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if (r != 0) return r;

	return 0;
}

void conn_accept_loop(int server, struct conn *head, pthread_rwlock_t *rwlock, void *(*handle_conn)(void *))
{
	int r;

	while (1) {
		int client = accept(server, NULL, 0);
		if (client == -1) goto cont;

		struct conn *conn = NULL;
		r = conn_new(&conn, client);
		if (r != 0) goto cont_conn;

		r = pthread_create(&conn->tid, NULL, handle_conn, conn);
		if (r != 0) goto cont_conn;

		r = conn_append(head, conn, rwlock);
		if (r != 0) cb_eperror(r);
		continue;

	cont_conn:
		conn_destroy(conn);
	cont:
		cb_perror(errno);
	}
}

int conn_cancel_all(struct conn *head, pthread_rwlock_t *rwlock)
{
	int r;
	struct conn *conn;
	pthread_t t;

	// Cancel all connection threads (always fetch next of dummy head)
	while (1) {
		r = pthread_rwlock_rdlock(rwlock);
		if (r != 0) return r;
		conn = head->next;
		r = pthread_rwlock_unlock(rwlock);
		if (r != 0) return r;
		if (conn == NULL) break;
		t = conn->tid;

		r = pthread_cancel(t);
		if (r != 0) return r;
		r = pthread_join(t, NULL);
		if (r != 0) return r;
	}

	return 0;
}

//void conn_print(struct conn *head)
//{
//	int i = 0;
//	for (struct conn *conn = head->next; conn != NULL; conn = conn->next) {
//		printf("%d\n", i++);
//		printf("tid = %zd\n", conn->tid);
//		printf("sockfd = %d\n", conn->sockfd);
//		printf("mutex = %d\n", conn->mutex);
//		printf("next = %p\n", conn->next);
//		printf("prev = %p\n", conn->prev);
//	}
//}
