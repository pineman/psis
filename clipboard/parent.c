#include <stdint.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "cb_common.h"

#include "clipboard.h"
#include "parent.h"

void validate_addr(char *addr, char *port, struct sockaddr_in *sockaddr)
{
	int r;

	r = inet_pton(AF_INET, addr, (void *) &sockaddr->sin_addr);
	if (r == 0) {
		fprintf(stderr, "Invalid IPv4 address (or not in the form ddd.ddd.ddd.ddd): %s\n", addr);
		exit(EXIT_FAILURE);
	}

	char *check = NULL;
	unsigned long test = 0;
	errno = 0;
	test = strtoul(port, &check, 10);
	if (errno != 0) cb_eperror(errno);
	if (check == addr || test == 0 || test > UINT16_MAX) {
		fprintf(stderr, "Port must be: 0 < <port> < %d\n", UINT16_MAX);
		exit(EXIT_FAILURE);
	}
	sockaddr->sin_port = htons((uint16_t) test);
}

int connect_parent(char *addr, char *port)
{
	int r;

	struct sockaddr_in parent_addr;
	socklen_t parent_addrlen = sizeof(struct sockaddr_in);
	parent_addr.sin_family = AF_INET;
	validate_addr(addr, port, &parent_addr);

	int parent = socket(AF_INET, SOCK_STREAM, 0);
	if (parent == -1) cb_eperror(errno);

	r = connect(parent, (struct sockaddr *) &parent_addr, parent_addrlen);
	if (r == -1) cb_eperror(errno);

	printf("Connected to parent remote clipboard %s:%d\n",
		inet_ntoa(parent_addr.sin_addr), (int) ntohs(parent_addr.sin_port));

	return parent;
}

void cleanup_serve_parent(void *arg)
{
	int r;

	puts("cancelling parent thread");

	int *parent = (int *) arg;
	close(*parent);

	r = pthread_mutex_lock(&mode_lock);
	if (r != 0) cb_eperror(r);
	// TODO: Now im the master!!
	master = true;
	r = pthread_mutex_unlock(&mode_lock);
	if (r != 0) cb_eperror(r);
}

void *serve_parent(void *arg)
{
	//int r;

	char **argv = (char **) arg;
	int parent = connect_parent(argv[2], argv[3]);

	pthread_cleanup_push(cleanup_serve_parent, &parent);

	/*
	recv(buf)
	// Got update from parent, write to my regions and send downwards to remotes
	// TODO: same as global update in local and remote?
	lock(regions[region]);
	reigons[region].buf = buf
	unlock(regions[region]);

	lock(global_update)
	for (remote in remotes) remote.send(buf)
	unlock(global_update)
	*/

	sleep(100);

	pthread_cleanup_pop(1);

	return NULL;
}
