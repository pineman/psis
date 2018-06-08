#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#include "conn.h"
#include "region.h"

// Helper struct to cleanup threads.
struct clean {
	char **data;
	struct conn *conn;
	int *arg;
};

/* Declare globals */
extern pthread_t main_tid;
extern pthread_t parent_serve_tid;

// Global array of regions
extern struct region regions[CB_NUM_REGIONS];

// Connection lists
// Dummy head node doubly-linked list
extern struct conn *child_conn_list;
extern pthread_rwlock_t child_conn_list_rwlock; // Protects child_conn_list
extern pthread_mutex_t update_mutex;
extern struct conn *app_conn_list;
extern pthread_rwlock_t app_conn_list_rwlock; // Protects app_conn_list

// Parent connection
extern struct conn *parent_conn;
// Are we in connected mode or are we the root (i.e. we have no parent)
extern bool root;
extern pthread_rwlock_t parent_conn_rwlock; // Protects parent_conn and root

void init_globals(void);
void free_globals(void);
void listen_sockets(char *argv[]);
void create_threads(int *parent_socket, int *children_socket, int *app_socket);
void main_cleanup(void *arg);

#define cb_perror(err) do { \
	char __buf__[1024]; \
	strerror_r(err, __buf__, sizeof(__buf__)); \
	fprintf(stderr, "\x1B[31m%s:%d %s(), errno = %d %s\n\x1B[0m", __FILE__, __LINE__-1, __func__, err, __buf__); \
} while(0);

#define cb_eperror(err) do { \
	cb_perror(err); \
	pthread_cancel(main_tid); \
} while(0);

#ifdef CB_DBG
	#define cb_log(format, ...) do { \
		char __buf__[1024]; \
		sprintf(__buf__, format, __VA_ARGS__); \
		fprintf(stderr, "LOG: %s:%d %s(), %s", __FILE__, __LINE__, __func__, __buf__); \
	} while(0);
#else
	#define cb_log(format, ...) do {} while(0);
#endif
