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

bool do_copy(int sockfd, uint8_t region, uint32_t data_size, char **data)
{
	int r;

	cb_log("%s", "[GOT] cmd copy\n");
	if (data_size == 0) { cb_log("%s", "got data_size == 0\n"); return false; } // TODO

	// Allocate space for data the app will send
	*data = malloc(data_size);
	if (*data == NULL) {
		// TODO inform the client we have no space for data
		cb_perror(errno);
	}

	// Receive data
	r = cb_recv(sockfd, *data, data_size);
	if (r == 0) { cb_log("app disconnect, r = %d, errno = %d\n", r, errno); return false; } // TODO
	if (r == -1) { cb_log("recv failed, r = %d, errno = %d\n", r, errno); return false; } // TODO
	cb_log("[GOT] data = %s\n", data);

	r = pthread_rwlock_rdlock(&mode_rwlock);
	if (r != 0) cb_eperror(r);
	bool mroot = root;
	r = pthread_rwlock_unlock(&mode_rwlock);
	if (r != 0) cb_eperror(r);
	if (!mroot) {
		// Send to parent conn
		copy_to_parent(region, data_size, *data);

		free(*data);
		*data = NULL; // don't double free
	}
	else {
		// We are the root, update our regions and send down to children
		update_region(region, data_size, data);
		copy_to_children(region);
	}

	return true;
}

void copy_to_parent(uint8_t region, uint32_t data_size, char *data)
{
	int r;
	bool success;

	r = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (r != 0) cb_eperror(r);

	// Critical section: Writing to the parent socket
	r = pthread_rwlock_rdlock(&mode_rwlock);
	if (r != 0) cb_eperror(r);
	r = pthread_mutex_lock(&parent_conn->mutex);
	if (r != 0) cb_eperror(r);

	success = cb_send_msg_data(parent_conn->sockfd, CB_CMD_COPY, region, data_size, data);

	// End Critical section: Unlock the parent socket
	r = pthread_mutex_unlock(&parent_conn->mutex);
	if (r != 0) cb_eperror(r);
	r = pthread_rwlock_unlock(&mode_rwlock);
	if (r != 0) cb_eperror(r);

	if (!success) {
		r = pthread_cancel(parent_serve_tid); // Will destroy the connection
		if (r != 0) cb_eperror(r);
		r = pthread_join(parent_serve_tid, NULL);
		if (r != 0) cb_eperror(r);
	}

	r = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if (r != 0) cb_eperror(r);
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

void copy_to_children(uint8_t region)
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
	bool success;
	while (child_conn != NULL) {
		// Critical section: Writing to a child socket
		r = pthread_mutex_lock(&child_conn->mutex);
		if (r != 0) cb_eperror(r);

		success = cb_send_msg_data(child_conn->sockfd, CB_CMD_COPY, region, data_size, data);

		// End Critical section: Unlock the child socket
		r = pthread_mutex_unlock(&child_conn->mutex);
		if (r != 0) cb_eperror(r);
		child_conn = child_conn->next;
		// TODO: maybe can be joined with some other function
		if (!success) {
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

bool paste_region_app(uint8_t region, int app)
{
	int r;
	bool success;

	// Critical section: reading from regions[region]
	r = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (r != 0) cb_eperror(r);
	r = pthread_rwlock_rdlock(&regions[region].rwlock);
	if (r != 0) cb_eperror(r);

	// No need to lock app sockfd because only one thread writes to it.
	success = cb_send_msg_data(app, CB_CMD_PASTE, region, regions[region].data_size, regions[region].data);

	// End Critical section: Unlock the region
	r = pthread_rwlock_unlock(&regions[region].rwlock);
	if (r != 0) cb_eperror(r);
	r = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if (r != 0) cb_eperror(r);

	return success;
}
