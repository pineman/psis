#include <stdint.h>

#include <unistd.h>
#include <sys/socket.h>
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
	if (errno != 0) emperror(errno);
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
	if (parent == -1) emperror(errno);

	r = connect(parent, (struct sockaddr *) &parent_addr, parent_addrlen);
	if (r == -1) emperror(errno);

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
	if (r != 0) emperror(r);
	connected_mode = false;
	r = pthread_mutex_unlock(&mode_lock);
	if (r != 0) emperror(r);
}

void *serve_parent(void *arg)
{
	int r;

	char **argv = (char **) arg;
	int parent = connect_parent(argv[2], argv[3]);

	pthread_cleanup_push(cleanup_serve_parent, &parent);

	sleep(100);

	pthread_cleanup_pop(1);

	pthread_cancel(pthread_self());
}
