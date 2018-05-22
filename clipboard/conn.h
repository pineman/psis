#pragma once

// Connection struct + doubly-linked list
struct conn {
	pthread_t tid;
	int sockfd;
	pthread_mutex_t mutex; // Protects writes to sockfd if needed
	struct conn *next;
	struct conn *prev;
};

int conn_new(struct conn **conn, int sockfd);
void conn_destroy(struct conn *conn);
int conn_append(struct conn *head, struct conn *conn, pthread_rwlock_t *rwlock);
int conn_remove(struct conn *rm, pthread_rwlock_t *rwlock);
void conn_accept_loop(int server, struct conn *head, pthread_rwlock_t *rwlock, void *(*handle_conn)(void *));
int conn_cancel_all(struct conn *head, pthread_rwlock_t *rwlock);

void conn_print(struct conn *head);
