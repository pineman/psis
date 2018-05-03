#include <stddef.h>

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
	// Maximum size of sockaddr_un.sun_path
	int path_buf_size = sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path);
	char path_buf[path_buf_size];

	// Get current working directory and concatenate with CB_SOCKET
	if (getcwd(path_buf, path_buf_size) == NULL) emperror(errno);
	strcat(path_buf, CB_SOCKET);
	strcpy(local_addr.sun_path, path_buf);
	socklen_t local_addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(local_addr.sun_path) + 1;

	r = unlink(local_addr.sun_path);
	if (r == -1 && errno != ENOENT) emperror(errno);
	r = bind(local_socket, (struct sockaddr *) &local_addr, local_addrlen);
	if (r == -1) emperror(errno);
	r = listen(local_socket, SOMAXCONN);
	if (r == -1) emperror(errno);

	printf("Listening for local applications on %s\n", local_addr.sun_path);

	return local_socket;
}

void clean_up(void *arg)
{
	int *local_socket = (int *) arg;
	printf("cleaning up\n");
	// TODO: cancel all our connections
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
	pthread_cleanup_push(clean_up, &local_socket);

	// Don't quit on errors inside the accept loop. It's ok if one connection fails
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

	return NULL;
}

void *serve_local_client(void *arg)
{
	int r;
	struct thread_args *targs = (struct thread_args *) arg;

	// Make thread non-joinable: accept loop is not waiting for us.
	r = pthread_detach(targs->thread_id);
	if (r != 0) emperror(r);

	// pthread_cleanup_push(clean_up, &local_socket);
	printf("new thread have client %d\n", targs->client);

	sleep(100);

	// pthread_cleanup_pop(1);
	close(targs->client);
	free(targs);

	return NULL;
}
