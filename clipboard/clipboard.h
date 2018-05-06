#pragma once

#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#define CB_MAX_REMOTE_CONN 1
#define CB_MAX_LOCAL_CONN 1

struct conn {
	int sockfd;
	pthread_mutex_t lock; // Protects writes to sockfd // TODO: can be rwlock?
	// TODO: lock per connection or globally per update?
	// TODO: pthread_t tid; // TO CANCEL THE THREAD
};

struct region {
	char data[CB_DATA_MAX_SIZE];
	//time_t last_updated_time; // TODO
	pthread_mutex_t lock; // Protects `data`
};

struct thread_args {
	pthread_t thread_id;
	int client;
};

// Global array of regions
extern struct region local_regions[CB_NUM_REGIONS];
// Are we in connected mode?
extern bool connected_mode;
extern pthread_mutex_t mode_lock; // protects `connected_mode`
