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
#include "app.h"

// Listen UNIX on `CB_SOCKET` for local app connections
int listen_local(void)
{
	int r; // Auxiliary variable just for return values

	int local_socket;
	local_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (local_socket == -1) emperror(errno);

	struct sockaddr_un local_addr;
	local_addr.sun_family = AF_UNIX;
	strcpy(local_addr.sun_path, CB_SOCKET);
	socklen_t local_addrlen = sizeof(local_addr);

	r = unlink(local_addr.sun_path);
	if (r == -1 && errno != ENOENT) emperror(errno);
	r = bind(local_socket, (struct sockaddr *) &local_addr, local_addrlen);
	if (r == -1) emperror(errno);
	r = listen(local_socket, SOMAXCONN);
	if (r == -1) emperror(errno);

	char path_buf[PATH_MAX];
	if (getcwd(path_buf, PATH_MAX) == NULL) emperror(errno);

	printf("Listening for local applications on %s/%s\n", path_buf, local_addr.sun_path);

	return local_socket;
}

void cleanup_app_accept(void *arg)
{
	int *local_socket = (int *) arg;

	puts("cancelling app accept thread");
	// TODO: cancel all our connections & threads

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

	// Don't exit() on errors inside the accept loop. It's ok if one connection fails
	while (1) {
		int client = accept(app_socket, NULL, 0);
		if (client == -1) { mperror(errno); continue; }

		struct thread_args *targs = malloc(sizeof(struct thread_args));
		if (targs == NULL) { mperror(errno); close(client); continue; }
		targs->client = client;

		r = pthread_create(&targs->thread_id, NULL, serve_app, targs);
		if (r != 0) { mperror(r); close(targs->client); free(targs); continue; }

		// TODO: add connection to list
	}

	pthread_cleanup_pop(1);
}

void cleanup_serve_app(void *arg)
{
	puts("cancelling app client thread");
	struct thread_args *targs = (struct thread_args *) arg;
	close(targs->client);
	free(arg);
}

// Don't exit on errors
void *serve_app(void *arg)
{
	int r;
	struct thread_args *targs = (struct thread_args *) arg;

	pthread_cleanup_push(cleanup_serve_app, arg);

	uint8_t cmd;
	uint8_t region;
	uint32_t data_size;
	char *data;
	int tmp;
	while (1) {
		r = recv_msg(targs->client, &cmd, &region, &data_size);
		if (r == 0) break; // TODO: client went bye bye
		printf("serve_app: cmd = %d, region = %d, data_size = %d\n", cmd, region, data_size);

		if (cmd == CB_CMD_COPY) {
			assert(data_size != 0);
			data = malloc(data_size);
			if (data == NULL) mperror(errno);
			r = erecv(targs->client, data, data_size);
			printf("serve_app: data = %s\n", data);
			tmp = data_size;
		}
		else if (cmd == CB_CMD_REQ_PASTE) {
			assert(data_size == 0);
			printf("serve_app: got req_paste\n");
			r = send_msg(targs->client, CB_CMD_PASTE, region, tmp);
			r = esend(targs->client, data, tmp);
		}
	}
	free(data); // TODO: remove

	// Make thread non-joinable: accept loop cleanup will not join() us because
	// we need to die right now.
	r = pthread_detach(targs->thread_id);
	if (r != 0) mperror(r);

	pthread_cleanup_pop(1);
	return NULL;

	/*
	switch(cmd) {
	case CB_CMD_COPY:
		if (connected_mode) {
			// Send to the parent
		}
		else {
			// im the master, do global update (serialize and send to childs)
			// global update means no one else can write to childs TODO: does that even happen?
			lock(regions[region]);
			reigons[region].buf = buf
			unlock(regions[region]);

			lock(global_update)
			for (child in child_conn) child.send(buf)
			unlock(global_update)
		break;
	case CB_CMD_PASTE:
	default:
		break;
	}*/
}
