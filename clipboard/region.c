#include <assert.h>

#include <pthread.h>

#include "clipboard.h"
#include "region.h"

// Initialize regions
void init_regions(void)
{
	int r;

	for (int i = 0; i < CB_NUM_REGIONS; i++) {
		regions[i].data = NULL;
		regions[i].data_size = 0;
		r = pthread_rwlock_init(&regions[i].rwlock, NULL);
		if (r != 0) cb_eperror(r);
	}
}

// Free regions
void destroy_regions(void)
{
	for (int i = 0; i < CB_NUM_REGIONS; i++) {
		free(regions[i].data);
		pthread_rwlock_destroy(&regions[i].rwlock);
	}
}

void update_region(uint8_t region, uint32_t data_size, char **data)
{
	int r;

	// Critical section: updating a region (write lock regions[region])
	r = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (r != 0) cb_eperror(r);
	r = pthread_rwlock_wrlock(&regions[region].rwlock);
	if (r != 0) cb_eperror(r);

	free(regions[region].data);
	regions[region].data = *data;
	*data = NULL; // don't double free
	regions[region].data_size = data_size;

	// End Critical section: unlock region
	r = pthread_rwlock_unlock(&regions[region].rwlock);
	if (r != 0) cb_eperror(r);
	r = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if (r != 0) cb_eperror(r);
}

void send_region_down(uint8_t region)
{
	int r;

	// Critical section: Sending copy of a region to all children (read lock
	// child_conn_list and regions[region] - region can't be altered while
	// we're sending because that would envolve free()ing its data and we
	// need that pointer to send to children).
	r = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (r != 0) cb_eperror(r);

	r = pthread_rwlock_rdlock(&regions[region].rwlock);
	if (r != 0) cb_eperror(r);
	uint32_t data_size = regions[region].data_size;
	char *data = regions[region].data;

	r = pthread_rwlock_rdlock(&child_conn_list_rwlock);
	if (r != 0) cb_eperror(r);

	// Send to all children
	struct conn *child_conn = child_conn_list->next;
	bool term_conn = false;
	while (child_conn != NULL) {
		// Critical section: Writing to a child socket
		r = pthread_mutex_lock(&child_conn->mutex);
		if (r != 0) cb_eperror(r);

		cb_log("[SEND] cmd = %d, region = %d, data_size = %d\n", CB_CMD_COPY, region, data_size);
		r = cb_send_msg(child_conn->sockfd, CB_CMD_COPY, region, data_size);
		if (r == -1) {
			// send_msg failed, terminate connection
			cb_log("send_msg failed r = %d, errno = %d\n", r, errno);
			term_conn = true;
			goto cont;
		}
		// Fatal error: we tried to send an invalid message
		assert(r != -2);

		cb_log("[SEND] data = %s\n", data);
		r = cb_send(child_conn->sockfd, data, data_size);
		if (r == -1) {
			// send data failed, terminate connection
			cb_log("send data failed r = %d, errno = %d\n", r, errno);
			term_conn = true;
			goto cont;
		}

	cont:
		// End Critical section: Unlock the child socket
		r = pthread_mutex_unlock(&child_conn->mutex);
		if (r != 0) cb_eperror(r);
		child_conn = child_conn->next;
		// TODO: maybe can be joined with some other function
		if (term_conn) {
			// Cancel the child's connection thread. Unlock our lock on
			// child_conn_list to allow the child's thread to die and cleanup.
			r = pthread_rwlock_unlock(&child_conn_list_rwlock);
			if (r != 0) cb_eperror(r);

			pthread_t t = child_conn->tid;
			r = pthread_cancel(t); // Will remove and destroy the connection
			if (r != 0) cb_eperror(r);
			r = pthread_join(t, NULL);
			if (r != 0) cb_eperror(r);

			// Continue looping
			r = pthread_rwlock_rdlock(&child_conn_list_rwlock);
			if (r != 0) cb_eperror(r);
			term_conn = false;
		}
	}

	// End Critical section: Unlock regions[region] and child_conn_list
	r = pthread_rwlock_unlock(&regions[region].rwlock);
	if (r != 0) cb_eperror(r);
	r = pthread_rwlock_unlock(&child_conn_list_rwlock);
	if (r != 0) cb_eperror(r);
	r = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if (r != 0) cb_eperror(r);
}
