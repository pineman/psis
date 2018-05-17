#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "cb_common.h"
#include "cb_msg.h"

#include "clipboard.h"
#include "child.h"

// Listen INET on a random port for child clipboard connections
int listen_child(void)
{
	int r; // Auxiliary variable just for return values

	int child_socket;
	child_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (child_socket == -1) cb_eperror(errno);

	struct sockaddr_in local_addr;
	//local_addr.sin_family = AF_INET;
	//local_addr.sin_port = 0;
	//local_addr.sin_addr.s_addr = INADDR_ANY;
	socklen_t local_addrlen = sizeof(struct sockaddr_in);

	//r = bind(child_socket, (struct sockaddr *) &local_addr, local_addrlen);
	//if (r == -1) cb_eperror(errno);
	r = listen(child_socket, SOMAXCONN);
	if (r == -1) cb_eperror(errno);

	r = getsockname(child_socket, (struct sockaddr *) &local_addr, &local_addrlen);
	if (r == -1) cb_eperror(errno);
	printf("Listening for child clipboards on %s:%d\n", inet_ntoa(local_addr.sin_addr), ntohs(local_addr.sin_port));

	return child_socket;
}

void *child_accept(void *arg)
{
	(void) arg;
	int r;
	int child_socket = listen_child();

	pthread_t t;
	while (1) {
		int c = accept( child_socket, NULL, 0);
		if (c == -1) {cb_eperror(errno); continue; }

		r = pthread_create(&t, NULL, serve_child, &c);
		if (r != 0) { cb_eperror(r); close (c); continue; }
	}

	close(child_socket);

	return NULL;
}

void *serve_child(void *arg)
{
	int c = *((int *) arg);

	printf("new thread have client %d\n", c);

	sleep(100);

	/*
	if (connected_mode) {
		// Send to the parent
		write(parent, buf);
	}
	else {
		// im the master, do global update (serialize and send to childs)
		lock(regions[region]);
		reigons[region].buf = buf
		unlock(regions[region]);

		lock(global_update)
		for (child in child_conn) child.send(buf)
		unlock(global_update)
	}*/

	close(c);

	return NULL;
}
