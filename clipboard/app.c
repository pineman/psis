#include <stddef.h>
#include <string.h>
#include <limits.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#include "clipboard.h"
#include "region.h"
#include "app.h"

// Listen UNIX on `CB_SOCKET` for local app connections
int listen_local(void)
{
	int r; // Auxiliary variable just for return values

	int local_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (local_socket == -1) return -1;

	struct sockaddr_un local_addr;
	local_addr.sun_family = AF_UNIX;
	strcpy(local_addr.sun_path, CB_SOCKET);
	socklen_t local_addrlen = sizeof(local_addr);

	r = unlink(local_addr.sun_path);
	if (r == -1 && errno != ENOENT) return -1;
	r = bind(local_socket, (struct sockaddr *) &local_addr, local_addrlen);
	if (r == -1) return -1;
	r = listen(local_socket, SOMAXCONN);
	if (r == -1) return -1;

	char path_buf[PATH_MAX];
	if (getcwd(path_buf, PATH_MAX) == NULL) return -1;

	printf("Listening for local applications on %s/%s\n", path_buf, local_addr.sun_path);

	return local_socket;
}

void *app_accept(void *arg)
{
	pthread_cleanup_push(cleanup_app_accept, arg);

	conn_accept_loop(*((int *) arg), app_conn_list, &app_conn_list_rwlock, serve_app);

	pthread_cleanup_pop(1);

	return NULL;
}

void cleanup_app_accept(void *arg)
{
	int r;
	int *local_socket = (int *) arg;

	cb_log("%s", "cleaning up app_accept\n");

	// Cancel all app threads
	r = conn_cancel_all(app_conn_list, &app_conn_list_rwlock);
	if (r != 0) cb_perror(r);

	// Close and unlink local socket (ignore return value since we're exiting
	// anyway).
	(void) close(*local_socket);
	(void) free(local_socket);
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
		cb_log("[GOT] cmd = %u, region = %u, data_size = %u\n", cmd, region, data_size);
		if (r == 0) { cb_log("%s", "app disconnect\n"); break; }
		if (r == -1) { cb_log("recv_msg failed r = %d, errno = %d\n", r, errno); break; }
		if (r == -2) { cb_log("recv_msg got invalid message r = %d, errno = %d\n", r, errno); break; }

		if (cmd == CB_CMD_COPY) {
			cb_log("%s", "[GOT] cmd copy\n");

			// do_copy: copy down if we don't have a parent (root = true); and we're the app
			r = do_copy(conn->sockfd, region, data_size, &data, root, true);
			if (r == false) break; // Terminate connection
		}
		else if (cmd == CB_CMD_REQ_PASTE) {
			cb_log("%s", "[GOT] cmd req_paste\n");
			if (data_size != 0) { cb_log("%s", "got data_size != 0\n"); break; }

			r = paste_region_app(region, conn->sockfd);
			if (r == false) break; // Terminate connection
		}
		else if (cmd == CB_CMD_REQ_WAIT) {
			cb_log("%s", "[GOT] cmd req_wait\n");
			if (data_size != 0) { cb_log("%s", "got data_size != 0\n"); break; }

			r = pthread_mutex_lock(&regions[region].update_mutex);
			if (r != 0) cb_eperror(r);
			r = pthread_cond_wait(&regions[region].update_cond, &regions[region].update_mutex);
			if (r != 0) cb_eperror(r);
			r = pthread_mutex_unlock(&regions[region].update_mutex);
			if (r != 0) cb_eperror(r);

			r = paste_region_app(region, conn->sockfd);
			if (r == false) break; // Terminate connection
		}
	}

	// Make thread non-joinable: accept loop cleanup will not join() us because
	// we need to die right now (our connection died).
	r = pthread_detach(pthread_self());
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

	cb_log("%s", "app cleanup\n");

	r = conn_remove(clean->conn, &app_conn_list_rwlock);
	if (r != 0) cb_perror(r);

	conn_destroy(clean->conn);
}
