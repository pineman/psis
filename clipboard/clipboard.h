#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#include "conn.h"

// Region struct
struct region {
	char *data; // Always changing on parent read()
	uint32_t data_size;
	pthread_rwlock_t rwlock; // Protects `data` and `data_size`
};

// Helper struct to cleanup threads.
struct clean {
	char **data;
	struct conn *conn;
};

/* Declare globals */
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
// Protects `parent_conn` and `root` boolean. They will only change once at
// most (if the parent connection dies) but they must still be protected.
pthread_rwlock_t mode_rwlock;

#define cb_perror(err) do { char __buf__[1024]; sprintf(__buf__, "%s:%d %s(), errno = %d", __FILE__, __LINE__-1, __func__, err); perror(__buf__); } while(0);
// TODO: can't exit because threads!
#define cb_eperror(err) do { cb_perror(err); exit(err); } while(0);
#define cb_log1(format, ...) do { char __buf__[1024]; sprintf(__buf__, format, __VA_ARGS__); fprintf(stderr, "LOG: %s:%d %s(), %s", __FILE__, __LINE__-1, __func__, __buf__); } while(0);
#define cb_log(format, ...) do {} while(0);
