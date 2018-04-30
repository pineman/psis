#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define eperror(r) { fprintf(stderr, "%s:%d errno = %d: %s\n", __FILE__, __LINE__-1, r, strerror(r)); exit(r); }

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define NUM_REGIONS 10
char *local_regions[NUM_REGIONS];

#define CLIPBOARD_SOCKET "/cb_sock"

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

	// Get current working directory and concatenate with CLIPBOARD_SOCKET
	if (getcwd(path_buf, path_buf_size) == NULL) eperror(errno);
	strcat(path_buf, CLIPBOARD_SOCKET);
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

int listen_remote(void)
{
	int r; // Auxiliary variable just for return values

	int remote_socket;
	remote_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (remote_socket == -1) eperror(errno);

	struct sockaddr_in local_addr;
	//local_addr.sin_family = AF_INET;
	//local_addr.sin_port = 0;
	//local_addr.sin_addr.s_addr = INADDR_ANY;
	socklen_t local_addrlen = sizeof(struct sockaddr_in);

	//r = bind(remote_socket, (struct sockaddr *) &local_addr, local_addrlen);
	//if (r == -1) eperror(errno);

	r = listen(remote_socket, SOMAXCONN);
	if (r == -1) eperror(errno);

	r = getsockname(remote_socket, (struct sockaddr *) &local_addr, &local_addrlen);
	if (r == -1) eperror(errno);
	printf("Listening for remote clipboards on %s:%d\n", inet_ntoa(local_addr.sin_addr), ntohs(local_addr.sin_port));

	return remote_socket;
}

void *local_accept_thread()
{
	// Listen UNIX on getcwd()/`CLIPBOARD_SOCKET`for local app connections
	int local_socket = listen_local();

	while (1) {
	}

	close(local_socket);
}

void *remote_accept_thread()
{
	// Listen INET on a random port for remote clipboard connections
	int remote_socket = listen_remote();

	while (1) {
	}

	close(remote_socket);
}

int main(int argc, char *argv[])
{
	int r;

	/* In order to launch the clipboard in connected mode it is necessary for
	 * the user to use the command  line  argument  -c followed  by  the
	 * address  and  port  of  another  clipboard.
	 * Although a clipboard can work in single or connected mode, it should be
	 * able to always receive connections from the local applications or remote
	 * clipboards. => All clipboards listen on a random port
	 */

	if (argc > 1) {
		if (argc != 4) {
			puts("-c <address> <port>");
			exit(1);
		}
		// Connected mode
		printf("Connecting to %s:%s\n", argv[2], argv[3]);
		// Connect to a parent remote clipboard given in argv[]
	}
	else {
		// Single mode
		// Don't connect to a parent clipboard (while still accepting connections
		// by potencial children clipboards on `remote_socket`)
	}

	pthread_t lt, rt;
	r = pthread_create(&lt, NULL, local_accept_thread, NULL);
	if (r != 0) eperror(r);
	r = pthread_create(&rt, NULL, remote_accept_thread, NULL);
	if (r != 0) eperror(r);

	r = pthread_join(lt, NULL);
	r = pthread_join(rt, NULL);

	return EXIT_SUCCESS;
}
