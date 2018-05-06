#include <stddef.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#include "clipboard.h"
#include "local.h"

// Listen UNIX on getcwd()/`CB_SOCKET`for local app connections
int listen_local(void)
{
	int r; // Auxiliary variable just for return values

	int local_socket;
	local_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (local_socket == -1) emperror(errno);

	struct sockaddr_un local_addr;
	local_addr.sun_family = AF_UNIX;
	char path_buf[sizeof(local_addr.sun_path)];
	// Get current working directory and concatenate with CB_SOCKET
	if (getcwd(path_buf, sizeof(local_addr.sun_path)) == NULL) emperror(errno);
	strcat(path_buf, CB_SOCKET);
	strcpy(local_addr.sun_path, path_buf);
	socklen_t local_addrlen = sizeof(local_addr);

	r = unlink(local_addr.sun_path);
	if (r == -1 && errno != ENOENT) emperror(errno);
	r = bind(local_socket, (struct sockaddr *) &local_addr, local_addrlen);
	if (r == -1) emperror(errno);
	r = listen(local_socket, SOMAXCONN);
	if (r == -1) emperror(errno);

	printf("Listening for local applications on %s\n", local_addr.sun_path);

	return local_socket;
}

void cleanup_local_accept_loop(void *arg)
{
	puts("cancelling local accept loop thread");
	int *local_socket = (int *) arg;
	// TODO: cancel all our connections/threads
	close(*local_socket);
}

void *local_accept_loop(void *arg) // TODO: abstract away into generic function?
// Args could be listen function, client handle function and array to put connections in to pass to clean_up
{
	int r;
	int local_socket = listen_local();

	// listen_local() is not a cancellation point therefore any cancellation
	// requests will only be reacted to after listen_local(), i.e. local_socket
	// is set.
	pthread_cleanup_push(cleanup_local_accept_loop, &local_socket);

	// Don't exit() on errors inside the accept loop. It's ok if one connection fails
	while (1) {
		int client = accept(local_socket, NULL, 0);
		if (client == -1) { mperror(errno); continue; }

		struct thread_args *targs = malloc(sizeof(struct thread_args));
		if (targs == NULL) { mperror(errno); continue; }
		targs->client = client;

		r = pthread_create(&targs->thread_id, NULL, serve_local_client, targs);
		if (r != 0) { mperror(r); close (targs->client); continue; }
	}

	pthread_cleanup_pop(1);
	pthread_exit(NULL);
	//return NULL;
}

void cleanup_local_client(void *arg)
{
	puts("cancelling local client thread");
	struct thread_args *targs = (struct thread_args *) arg;
	close(targs->client);
	free(arg);
}

void *serve_local_client(void *arg)
{
	int r;
	struct thread_args *targs = (struct thread_args *) arg;

	// Make thread non-joinable: accept loop is not waiting for us.
	r = pthread_detach(targs->thread_id);
	if (r != 0) emperror(r);

	pthread_cleanup_push(cleanup_local_client, arg);

	char *data1 = malloc(CB_DATA_MAX_SIZE);
	char *data2 = malloc(CB_DATA_MAX_SIZE);

	uint8_t cmd;
	uint8_t region;
	int s;
	r = recv_msg(targs->client, &cmd, &region, CB_DATA_MAX_SIZE, data1);
	s = r;

	r = recv_msg(targs->client, &cmd, &region, CB_DATA_MAX_SIZE, data2);

	r = send_msg(targs->client, CB_CMD_PASTE, region, s, data1);

	free(data1);
	free(data2);

	pthread_cleanup_pop(1);
	return NULL;
}
