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

void *serve_parent(void *arg)
{
	int r;

	struct thread_args *targs = (struct thread_args *) arg;

	// Make thread non-joinable: no one is not waiting for us.
	r = pthread_detach(targs->thread_id);
	if (r != 0) emperror(r);

	close(targs->client);

	free(targs);
}
