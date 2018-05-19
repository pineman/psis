#include <assert.h>

#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "clipboard.h"
#include "utils.h"

int block_signals(sigset_t *sigset)
{
	int r;

	r = sigfillset(sigset);
	if (r == -1) return -1;

	// Don't ignore stop/continue signals
	r = sigdelset(sigset, SIGSTOP);
	if (r == -1) return -1;
	r = sigdelset(sigset, SIGTSTP);
	if (r == -1) return -1;
	r = sigdelset(sigset, SIGCONT);
	if (r == -1) return -1;

	// Set thread sigmask
	r = pthread_sigmask(SIG_SETMASK, sigset, NULL);
	if (r == -1) return -1;

	return 0;
}

int init_mutex(pthread_mutex_t *mutex)
{
	int r;
	pthread_mutexattr_t mutex_attr;
	r = pthread_mutexattr_init(&mutex_attr);
	if (r != 0) return r;

#ifdef PTHREAD_MUTEX_ERRORCHECK
	r = pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
	if (r != 0) return r;
#endif
#ifdef PTHREAD_MUTEX_ROBUST
	r = pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST);
	if (r != 0) return r;
#endif

	r = pthread_mutex_init(mutex, &mutex_attr);
	if (r != 0) return r;

	r = pthread_mutexattr_destroy(&mutex_attr);
	if (r != 0) return r;

	return 0;
}

int conn_new(struct conn **conn, int sockfd)
{
	int r;

	*conn = malloc(sizeof(struct conn));
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

void conn_append(struct conn *head, struct conn *new)
{
	// TODO: remove assert
	assert(head != NULL);
	assert(head->prev == NULL);
	assert(new != NULL);

	// Prev
	struct conn *old = head->next;
	if (old != NULL) old->prev = new;
	new->next = old;
	// Next
	head->next = new;
	new->prev = head;
}

void conn_remove(struct conn *rm)
{
	assert(rm != NULL);

	if (rm->next) rm->next->prev = rm->prev;
	if (rm->prev) rm->prev->next = rm->next;
}

void conn_print(struct conn *head)
{
	int i = 0;
	for (struct conn *conn = head->next; conn != NULL; conn = conn->next) {
		printf("%d\n", i++);
		printf("tid = %zd\n", conn->tid);
		printf("sockfd = %d\n", conn->sockfd);
		printf("mutex = %d\n", conn->mutex);
		printf("next = %p\n", conn->next);
		printf("prev = %p\n", conn->prev);
	}
}
