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
int conn_append(struct conn *head, struct conn *conn);
int conn_remove(struct conn *rm);

void conn_print(struct conn *head);
