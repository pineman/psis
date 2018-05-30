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
	if (errno != 0) { cb_perror(errno); exit(EXIT_FAILURE); }
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
	if (parent == -1) return -1;

	r = connect(parent, (struct sockaddr *) &parent_addr, parent_addrlen);
	if (r == -1) return -1;

	printf("Connected to parent remote clipboard %s:%d\n",
		inet_ntoa(parent_addr.sin_addr), (int) ntohs(parent_addr.sin_port));

	return parent;
}

void *serve_parent(void *arg)
{
	int r;
	int parent = *((int *) arg);
	r = conn_new(&parent_conn, parent);
	if (r != 0) cb_eperror(r);

	uint8_t cmd = 0;
	uint8_t region = 0;
	uint32_t data_size = 0;
	char *data = NULL;

	struct clean clean = {.data = &data, .conn = parent_conn, .arg = arg };
	pthread_cleanup_push(cleanup_serve_parent, &clean);

	int init_msg = 0;
	bool initial = true;
	while (1)
	{
		r = cb_recv_msg(parent_conn->sockfd, &cmd, &region, &data_size);
		cb_log("[GOT] cmd = %d, region = %d, data_size = %d\n", cmd, region, data_size);
		if (r == 0) { cb_log("%s", "parent disconnect\n"); break; }
		if (r == -1) { cb_log("recv_msg failed r = %d, errno = %d\n", r, errno); break; }
		if (r == -2) { cb_log("recv_msg got invalid message r = %d, errno = %d\n", r, errno); break; }

		// Can only receive copy from parent. Terminate connection if not
		if (cmd != CB_CMD_COPY) {
			cb_log("%s", "cmd != CB_CMD_COPY\n");
			break;
		}

		// Initial sync: Receiving CB_NUM_REGIONS copy messages.
		if (init_msg < CB_NUM_REGIONS)
			init_msg++;
		else
			initial = false;

		// Parent has sent us a copy: update regions and send to children.
		r = do_copy(parent_conn->sockfd, region, data_size, &data, true, initial);
		if (r == false) { cb_log("%s", "parent died\n"); break; }// Terminate connection
	}

	// Make thread non-joinable: no one will join() us because we need to die
	// right now (our connection to parent died).
	r = pthread_detach(pthread_self());
	if (r != 0) cb_perror(r);

	pthread_cleanup_pop(1);

	return NULL;
}

void cleanup_serve_parent(void *arg)
{
	int r;
	struct clean *clean = (struct clean *) arg;
	if (*clean->data != NULL) free(*clean->data);
	free(clean->arg);

	cb_log("%s", "parent cleanup\n");

	r = pthread_rwlock_wrlock(&parent_conn_rwlock);
	if (r != 0) cb_eperror(r);
	// Parent connection dead, now we are the root.
	conn_destroy(parent_conn);
	root = true;
	parent_conn = NULL;
	r = pthread_rwlock_unlock(&parent_conn_rwlock);
	if (r != 0) cb_eperror(r);
}
