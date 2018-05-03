#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#include "clipboard.h"
#include "remote.h"

// Listen INET on a random port for remote clipboard connections
int listen_remote(void)
{
	int r; // Auxiliary variable just for return values

	int remote_socket;
	remote_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (remote_socket == -1) emperror(errno);

	struct sockaddr_in local_addr;
	//local_addr.sin_family = AF_INET;
	//local_addr.sin_port = 0;
	//local_addr.sin_addr.s_addr = INADDR_ANY;
	socklen_t local_addrlen = sizeof(struct sockaddr_in);

	//r = bind(remote_socket, (struct sockaddr *) &local_addr, local_addrlen);
	//if (r == -1) emperror(errno);
	r = listen(remote_socket, SOMAXCONN);
	if (r == -1) emperror(errno);

	r = getsockname(remote_socket, (struct sockaddr *) &local_addr, &local_addrlen);
	if (r == -1) emperror(errno);
	printf("Listening for remote clipboards on %s:%d\n", inet_ntoa(local_addr.sin_addr), ntohs(local_addr.sin_port));

	return remote_socket;
}

void *remote_accept_loop(void *arg)
{
	int r;
	int remote_socket = listen_remote();

	pthread_t t;
	while (1) {
		int c = accept(remote_socket, NULL, 0);
		if (c == -1) { mperror(errno); continue; }

		r = pthread_create(&t, NULL, serve_remote_client, &c);
		if (r != 0) { mperror(r); close (c); continue; }
	}

	close(remote_socket);

	return NULL;
}

void *serve_remote_client(void *arg)
{
	int c = *((int *) arg);

	printf("new thread have client %d\n", c);

	close(c);

	return NULL;
}
