#pragma once

#include <signal.h>
#include <pthread.h>

struct clean {
	char **data;
	struct conn *conn;
};

int block_signals(sigset_t *sigset);
int init_mutex(pthread_mutex_t *mutex);
int conn_new(struct conn **conn, int sockfd);
void conn_destroy(struct conn *conn);
void conn_append(struct conn *head, struct conn *new);
void conn_remove(struct conn *rm);
void conn_print(struct conn *head);
