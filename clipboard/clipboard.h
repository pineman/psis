#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#define cb_perror(err) do { char __buf__[1024]; sprintf(__buf__, "%s:%d %s(), errno = %d", __FILE__, __LINE__-1, __func__, err); perror(__buf__); } while(0);
// TODO: can't exit because threads!
#define cb_eperror(err) do { cb_perror(err); exit(err); } while(0);

#define CB_MAX_REMOTE_CONN 1
#define CB_MAX_LOCAL_CONN 1

struct conn {
	int sockfd;
	pthread_mutex_t lock; // Protects writes to sockfd // TODO: can be rwlock?
	// TODO: lock per connection or globally per update?
	pthread_t tid; // TO CANCEL THE THREAD
	struct conn *next;
};

struct region {
	char *data; // Always changing on parent read()
	pthread_mutex_t lock; // Protects `data` TODO: rwlock
};

struct thread_args {
	pthread_t thread_id;
	int client;
};

// Global array of regions
extern struct region app_regions[CB_NUM_REGIONS];

// Global array of remote connections
extern struct conn *child_conn;
// Global array of local connections
extern struct conn *app_conn;

extern struct conn *parent_conn;

// Are we in connected mode or are we the master (i.e. we have no parent)
extern bool master;
extern pthread_mutex_t mode_lock; // protects `master` boolean
