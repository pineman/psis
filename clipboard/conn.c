#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>
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

	// Prev
	struct conn *old = head->next;
	if (old != NULL) old->prev = new;
	new->next = old;
	// Next
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
