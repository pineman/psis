#include <assert.h>

#include <pthread.h>

#include "clipboard.h"
#include "region.h"

// Initialize regions
void init_regions(void)
{
	int r;

	for (int i = 0; i < CB_NUM_REGIONS; i++) {
		regions[i].data = malloc(1);
		regions[i].data[0] = '\0';
		regions[i].data_size = 1;
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

bool do_copy(int sockfd, uint8_t region, uint32_t data_size, char **data, bool copy_down, bool initial)
{
	int r;

	cb_log("%s", "[GOT] cmd copy\n");
	if (data_size == 0) { cb_log("%s", "got data_size == 0\n"); return false; }

	// Allocate space for data the app will send
	*data = malloc(data_size);
	if (*data == NULL) {
		// TODO inform the client we have no space for data
		cb_perror(errno);
	}

	// Receive data
	r = cb_recv(sockfd, *data, data_size);
	if (r == 0) { cb_log("disconnect, r = %d, errno = %d\n", r, errno); return false; }
	if (r == -1) { cb_log("recv failed, r = %d, errno = %d\n", r, errno); return false; }
	cb_log("[GOT] data = %s\n", *data);

	if (!copy_down) {
		// Send to parent conn
		cb_log("%s", "copy to parent\n");
		copy_to_parent(region, data_size, *data);

		free(*data);
		*data = NULL; // don't double free
	}
	else {
		// Update our region and send down to children (copy down)
		cb_log("%s", "update region copy to children\n");
		update_region(region, data_size, data);
		if (!initial) {
			copy_to_children(region);
		}
	}

	return true;
}

void copy_to_parent(uint8_t region, uint32_t data_size, char *data)
{
	int r;

	// Critical section: Writing to the parent socket
	r = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (r != 0) cb_eperror(r);
	r = pthread_rwlock_rdlock(&parent_conn_rwlock);
	if (r != 0) cb_eperror(r);
	if (root) return; // Parent died between call to do_copy and now!
	r = pthread_mutex_lock(&parent_conn->mutex);
	if (r != 0) cb_eperror(r);

	// Try to send update to parent. Ignore if it fails - if the connection to the parent died,
	// it will be cancelled once it eventually returns to its receive loop.
	(void) cb_send_msg_data(parent_conn->sockfd, CB_CMD_COPY, region, data_size, data);

	// End Critical section: unlock parent socket
	r = pthread_mutex_unlock(&parent_conn->mutex);
	if (r != 0) cb_eperror(r);
	r = pthread_rwlock_unlock(&parent_conn_rwlock);
	if (r != 0) cb_eperror(r);
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
	while (child_conn != NULL) {
		// Critical section: Writing to a child socket
		r = pthread_mutex_lock(&child_conn->mutex);
		if (r != 0) cb_eperror(r);

		// Try to send update to child. Ignore if it fails - if the connection to that child died,
		// it will be cancelled once it eventually returns to its receive loop.
		(void) cb_send_msg_data(child_conn->sockfd, CB_CMD_COPY, region, data_size, data);
		cb_log("[SENT] cmd = %d, region = %d, data_size = %d, data = %s\n", CB_CMD_COPY, region, data_size, data);

		// End Critical section: Unlock the child socket
		r = pthread_mutex_unlock(&child_conn->mutex);
		if (r != 0) cb_eperror(r);

		child_conn = child_conn->next;
	}

	// End Critical section: Unlock regions[region] and child_conn_list
	r = pthread_rwlock_unlock(&child_conn_list_rwlock);
	if (r != 0) cb_eperror(r);
	r = pthread_rwlock_unlock(&regions[region].rwlock);
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
