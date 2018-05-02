#include <stddef.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"
#include "local.h"

// Listen UNIX on getcwd()/`CB_SOCKET`for local app connections
int listen_local(void)
{
	int r; // Auxiliary variable just for return values

	int local_socket;
	local_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (local_socket == -1) eperror(errno);

	struct sockaddr_un local_addr;
	local_addr.sun_family = AF_UNIX;
	// Maximum size of sockaddr_un.sun_path
	int path_buf_size = sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path);
	char path_buf[path_buf_size];

	// Get current working directory and concatenate with CB_SOCKET
	if (getcwd(path_buf, path_buf_size) == NULL) eperror(errno);
	strcat(path_buf, CB_SOCKET);
	strcpy(local_addr.sun_path, path_buf);
	socklen_t local_addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(local_addr.sun_path) + 1;

	r = unlink(local_addr.sun_path);
	if (r == -1 && errno != ENOENT) eperror(errno);
	r = bind(local_socket, (struct sockaddr *) &local_addr, local_addrlen);
	if (r == -1) eperror(errno);
	r = listen(local_socket, SOMAXCONN);
	if (r == -1) eperror(errno);

	printf("Listening for local applications on %s\n", local_addr.sun_path);

	return local_socket;
}

void *local_accept_thread(void *arg) // TODO: abstract away into generic function?
{
	int r;
	int local_socket = listen_local();

	pthread_t t;
	while (1) {
		int c = accept(local_socket, NULL, 0);
		if (c == -1) { mperror(errno); continue; }

		r = pthread_create(&t, NULL, serve_local_client, &c);
		if (r != 0) { mperror(r); close (c); continue; }
	}

	close(local_socket);

	return NULL;
}

void *serve_local_client(void *arg)
{
	int c = *((int *) arg);

	printf("new thread have client %d\n", c);

	close(c);

	return NULL;
}
