#include <assert.h>

#include <pthread.h>

#include "clipboard.h"
#include "region.h"
#include "utils.h"

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
		r = init_mutex(&regions[i].update_mutex);
		if (r != 0) cb_eperror(r);
		r = pthread_cond_init(&regions[i].update_cond, NULL);
		if (r != 0) cb_eperror(r);
	}
}

// Free regions
void destroy_regions(void)
{
	for (int i = 0; i < CB_NUM_REGIONS; i++) {
		free(regions[i].data);
		(void) pthread_rwlock_destroy(&regions[i].rwlock);
		(void) pthread_mutex_destroy(&regions[i].update_mutex);
		(void) pthread_cond_destroy(&regions[i].update_cond);
	}
}

// This function handles copy commands for all threads.
// The parent thread always copies new updates down to children,
// and the child and app threads do as well if the clipboard is in single mode.
// If the clipboard is in connected mode, they send new updates to the parent (copy_down == false).
bool do_copy(int sockfd, uint8_t region, uint32_t data_size, char **data, bool copy_down, bool app)
{
	int r;

	cb_log("%s", "[GOT] cmd copy\n");
	if (data_size == 0) { cb_log("%s", "got data_size == 0\n"); return false; }

	// Allocate space for data the client will send
	cb_log("data_size = %u\n", data_size);
	*data = malloc(data_size);
	if (*data == NULL) {
		// No memory for app request
		cb_perror(errno);
		if (app) {
			r = cb_send_msg(sockfd, CB_CMD_RESP_COPY_NOK, region, 0);
			if (r == -1) return false; // Connection died
			assert(r != -2);
			cb_log("%s", "sent NOK\n");
			// Although the server has no memory for this request, continue serving future requests
			return true;
		}
	}
	if (app) {
		r = cb_send_msg(sockfd, CB_CMD_RESP_COPY_OK, region, 0);
		if (r == -1) return false; // Connection died
		assert(r != -2);
		cb_log("%s", "sent OK\n");
	}

	// Receive data
	r = cb_recv(sockfd, *data, data_size);
	if (r == 0) { cb_log("disconnect, r = %d\n", r); return false; }
	if (r == -1) { cb_log("recv failed, r = %d, errno = %d\n", r, errno); return false; }
	// cb_log("[GOT] data = %s\n", *data);

	if (copy_down) {
		// Update our region and send down to children (copy down)
		cb_log("%s", "update region copy to children\n");
		update_region(region, data_size, data);
		copy_to_children(region);
	}
	else {
		// Send to parent connection
		cb_log("%s", "copy to parent\n");
		copy_to_parent(region, data_size, *data);

		free(*data);
		*data = NULL; // don't double free
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
	*data = NULL; // don't double free inside cleanup handler
	regions[region].data_size = data_size;

	// End Critical section: unlock region
	r = pthread_rwlock_unlock(&regions[region].rwlock);
	if (r != 0) cb_eperror(r);
	r = pthread_cond_broadcast(&regions[region].update_cond);
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

	r = pthread_mutex_lock(&update_mutex);
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
		cb_log("[SENT] cmd = %u, region = %u, data_size = %u, data = %s\n", CB_CMD_COPY, region, data_size, data);

		// End Critical section: Unlock the child socket
		r = pthread_mutex_unlock(&child_conn->mutex);
		if (r != 0) cb_eperror(r);

		child_conn = child_conn->next;
	}

	r = pthread_mutex_unlock(&update_mutex);
	if (r != 0) cb_eperror(r);

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
