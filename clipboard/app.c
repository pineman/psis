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

void cleanup_app_accept(void *arg)
{
	cb_log("%s", "cleaning up app_accept\n");
	int r;
	int *local_socket = (int *) arg;
	struct conn *conn;
	pthread_t t;

	conn = app_conn_list->next;
	while (conn != NULL) {
		t = conn->tid;
		conn = conn->next;
		r = pthread_cancel(t);
		if (r != 0) cb_eperror(r);
		r = pthread_join(t, NULL);
		if (r != 0) cb_eperror(r);
	}

	close(*local_socket);
	unlink(CB_SOCKET);
}

void *app_accept(void *arg) // TODO: abstract away into generic function?
// Args could be listen function, client handle function and array to put connections in to pass to clean_up
{
	(void) arg;
	int r;
	int app_socket = listen_local();

	pthread_cleanup_push(cleanup_app_accept, &app_socket);

	// Don't exit() on errors inside the accept loop. If one connection fails,
	// just continue accept()
	while (1) {
		int app = accept(app_socket, NULL, 0);
		if (app == -1) goto cont;

		struct conn *conn = NULL;
		r = conn_new(&conn, app);
		if (r != 0) goto cont_conn;

		r = pthread_create(&conn->tid, NULL, serve_app, conn);
		if (r != 0) goto cont_conn;

		conn_append(app_conn_list, conn);
		continue;

	cont_conn:
		conn_destroy(conn);
	cont:
		cb_perror(errno);
	}

	pthread_cleanup_pop(1);
}

void cleanup_serve_app(void *arg)
{
	puts("cleaning serve app");
	struct clean *clean = (struct clean *) arg;
	if (*clean->data != NULL) free(*clean->data);
	conn_remove(clean->conn);
	conn_destroy(clean->conn);
}

// Don't exit on errors
void *serve_app(void *arg)
{
	int r;
	struct conn *conn = (struct conn *) arg;

	uint8_t cmd = 0;
	uint8_t region = 0;
	uint32_t data_size = 0;
	char *data = NULL;
	uint32_t tmp = 0;

	struct clean clean = {.data = &data, .conn = conn};
	pthread_cleanup_push(cleanup_serve_app, &clean);

	while (1)
	{
		r = cb_recv_msg(conn->sockfd, &cmd, &region, &data_size);
		if (r == 0) break; // TODO: client went bye bye
		cb_log("[GOT] serve_app: cmd = %d, region = %d, data_size = %d\n", cmd, region, data_size);

		if (cmd == CB_CMD_COPY) {
			cb_log("%s", "[GOT] cmd copy\n");
			// TODO: assert must be pthread_cancel or something
			assert(data_size != 0);

			data = malloc(data_size);
			if (data == NULL) cb_perror(errno);
			r = cb_recv(conn->sockfd, data, data_size);
			cb_log("[GOT] data = %s, r = %d, errno = %d\n", data, r, errno);
			if (r == 0) break; // TODO: client went bye bye
			tmp = data_size;

			if (!root) {
				// Send to parent conn
			}
			else {
				// im the root, do global update (serialize and send to childs)
				// global update means no one else can write to childs TODO: does that even happen?
				/*
				lock(regions[region]);
				reigons[region].buf = buf
				unlock(regions[region]);

				lock(global_update)
				for (child in child_conn) child.send(buf)
				unlock(global_update)
				*/
			}
			//free(data); // TODO: uncomment
			//data = NULL;
		}
		else if (cmd == CB_CMD_REQ_PASTE) {
			assert(data_size == 0);
			cb_log("%s", "[GOT] cmd req_paste\n");
			r = cb_send_msg(conn->sockfd, CB_CMD_PASTE, region, tmp);
			cb_log("[SEND] serve_app: cmd = %d, region = %d, data_size = %d, data = %s\n", CB_CMD_PASTE, region, tmp, data);
			r = cb_send(conn->sockfd, data, tmp);
			cb_log("[SEND] data = %s, r = %d, errno = %d\n", data, r, errno);
		}
	}

	// Make thread non-joinable: accept loop cleanup will not join() us because
	// we need to die right now.
	r = pthread_detach(conn->tid);
	if (r != 0) cb_perror(r);

	pthread_cleanup_pop(1);

	return NULL;
}
