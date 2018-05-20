#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#include "clipboard.h"
#include "region.h"
#include "utils.h"
#include "app.h"

// Listen UNIX on `CB_SOCKET` for local app connections
int listen_local(void)
{
	int r; // Auxiliary variable just for return values

	int local_socket;
	local_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (local_socket == -1) cb_eperror(errno);

	struct sockaddr_un local_addr;
	local_addr.sun_family = AF_UNIX;
	strcpy(local_addr.sun_path, CB_SOCKET);
	socklen_t local_addrlen = sizeof(local_addr);

	r = unlink(local_addr.sun_path);
	if (r == -1 && errno != ENOENT) cb_eperror(errno);
	r = bind(local_socket, (struct sockaddr *) &local_addr, local_addrlen);
	if (r == -1) cb_eperror(errno);
	r = listen(local_socket, SOMAXCONN);
	if (r == -1) cb_eperror(errno);

	char path_buf[PATH_MAX];
	if (getcwd(path_buf, PATH_MAX) == NULL) cb_eperror(errno);

	printf("Listening for local applications on %s/%s\n", path_buf, local_addr.sun_path);

	return local_socket;
}

void *app_accept(void *arg) // TODO: abstract away into generic function?
// Args could be listen function, client handle function and array to put connections in to pass to clean_up
{
	(void) arg;
	int r;
	int app_socket = listen_local();

	pthread_cleanup_push(cleanup_app_accept, &app_socket);

	while (1) {
		int app = accept(app_socket, NULL, 0);
		if (app == -1) goto cont;

		struct conn *conn = NULL;
		r = conn_new(&conn, app);
		if (r != 0) goto cont_conn;

		r = pthread_create(&conn->tid, NULL, serve_app, conn);
		if (r != 0) goto cont_conn;

		r = conn_append(app_conn_list, conn, &app_conn_list_rwlock);
		if (r != 0) cb_eperror(r);
		continue;

	cont_conn:
		conn_destroy(conn);
	cont:
		cb_perror(errno);
	}

	pthread_cleanup_pop(1);
}

void cleanup_app_accept(void *arg)
{
	int r;
	int *local_socket = (int *) arg;
	struct conn *conn;
	pthread_t t;

	cb_log("%s", "cleaning up app_accept\n");

	// Transverse app connections list and cancel threads
	conn = app_conn_list->next;
	while (conn != NULL) {
		t = conn->tid;
		conn = conn->next;
		r = pthread_cancel(t);
		if (r != 0) cb_eperror(r);
		r = pthread_join(t, NULL);
		if (r != 0) cb_eperror(r);
	}

	// Close and unlink local socket
	close(*local_socket);
	unlink(CB_SOCKET);
}

// Serve one app connection
void *serve_app(void *arg)
{
	int r;
	struct conn *conn = (struct conn *) arg;

	uint8_t cmd = 0;
	uint8_t region = 0;
	uint32_t data_size = 0;
	char *data = NULL;

	struct clean clean = {.data = &data, .conn = conn};
	pthread_cleanup_push(cleanup_serve_app, &clean);

	while (1)
	{
		r = cb_recv_msg(conn->sockfd, &cmd, &region, &data_size);
		if (r == 0) { cb_log("%s", "app disconnect\n"); break; }
		if (r == -1) { cb_log("recv_msg failed r = %d, errno = %d\n", r, errno); break; } // TODO
		if (r == -2) { cb_log("recv_msg got invalid message r = %d, errno = %d\n", r, errno); break; } // TODO
		cb_log("[GOT] cmd = %d, region = %d, data_size = %d\n", cmd, region, data_size);

		if (cmd == CB_CMD_COPY) {
			cb_log("%s", "[GOT] cmd copy\n");
			if (data_size == 0) { cb_log("%s", "got data_size == 0\n"); break; } // TODO

			// Allocate space for data app will send
			data = malloc(data_size);
			if (data == NULL) {
				// TODO inform the client we have no space for data
				cb_perror(errno);
				continue;
			}

			// Receive data from app
			r = cb_recv(conn->sockfd, data, data_size);
			if (r == 0) { cb_log("app disconnect, r = %d, errno = %d\n", r, errno); break; }
			if (r == -1) { cb_log("recv failed, r = %d, errno = %d\n", r, errno); break; }
			cb_log("[GOT] data = %s\n", data);

			if (!root) {
				// Send to parent conn
				free(data);
				data = NULL; // don't double free
			}
			else {
				// we are the root, update region and send down to children
				update_region(region, data_size, &data);
				send_region_down(region);
			}
		}
		else if (cmd == CB_CMD_REQ_PASTE) {
			cb_log("%s", "[GOT] cmd req_paste\n");
			if (data_size != 0) { cb_log("%s", "got data_size != 0\n"); break; } // TODO

			// Critical section: reading from regions[region]
			r = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
			if (r != 0) cb_eperror(r);
			r = pthread_rwlock_rdlock(&regions[region].rwlock);
			if (r != 0) cb_eperror(r);

			cb_log("[SEND] cmd = %d, region = %d, data_size = %d, r = %d, errno = %d\n", CB_CMD_PASTE, region, regions[region].data_size, r, errno);
			r = cb_send_msg(conn->sockfd, CB_CMD_PASTE, region, regions[region].data_size);
			if (r == -1) {
				cb_log("send_msg failed r = %d, errno = %d\n", r, errno);
				// Connection died!
				r = pthread_rwlock_unlock(&regions[region].rwlock);
				if (r != 0) cb_eperror(r);
				r = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
				if (r != 0) cb_eperror(r);
				break;
			} // TODO
			if (r == -2) {
				cb_log("send_msg invalid message r = %d, errno = %d\n", r, errno);
				// Kill connection!
				r = pthread_rwlock_unlock(&regions[region].rwlock);
				if (r != 0) cb_eperror(r);
				r = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
				if (r != 0) cb_eperror(r);
				break;
			} // TODO

			cb_log("[SEND] data = %s, r = %d, errno = %d\n", regions[region].data, r, errno);
			r = cb_send(conn->sockfd, regions[region].data, regions[region].data_size);
			if (r == -1) {
				cb_log("%s", "send data failed\n");
				// Connection died!
				r = pthread_rwlock_unlock(&regions[region].rwlock);
				if (r != 0) cb_eperror(r);
				r = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
				if (r != 0) cb_eperror(r);
				break;
			} // TODO

			// End Critical section: Unlock the region
			r = pthread_rwlock_unlock(&regions[region].rwlock);
			if (r != 0) cb_eperror(r);
			r = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
			if (r != 0) cb_eperror(r);
		}
		else if (cmd == CB_CMD_WAIT) {
		}
	}

	// Make thread non-joinable: accept loop cleanup will not join() us because
	// we need to die right now.
	r = pthread_detach(conn->tid);
	if (r != 0) cb_perror(r);

	pthread_cleanup_pop(1);

	return NULL;
}

// Free temporary data buffer & connection and remove connection from global list.
void cleanup_serve_app(void *arg)
{
	int r;
	struct clean *clean = (struct clean *) arg;
	if (*clean->data != NULL) free(*clean->data);

	r = conn_remove(clean->conn, &app_conn_list_rwlock);
	if (r != 0) cb_eperror(r);

	conn_destroy(clean->conn);
}

