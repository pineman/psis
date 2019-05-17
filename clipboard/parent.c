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

	while (1)
	{
		r = cb_recv_msg(parent_conn->sockfd, &cmd, &region, &data_size);
		cb_log("[GOT] cmd = %u, region = %u, data_size = %u\n", cmd, region, data_size);
		if (r == 0) { cb_log("%s", "parent disconnect\n"); break; }
		if (r == -1) { cb_log("recv_msg failed r = %d, errno = %d\n", r, errno); break; }
		if (r == -2) { cb_log("recv_msg got invalid message r = %d, errno = %d\n", r, errno); break; }

		// Can only receive copy from parent. Terminate connection if not
		if (cmd != CB_CMD_COPY) {
			cb_log("%s", "cmd != CB_CMD_COPY\n");
			break;
		}

		// Parent has sent us a copy: update regions and send to children.
		// do_copy: always copy down (we're the parent thread); and we're not the app
		r = do_copy(parent_conn->sockfd, region, data_size, &data, true, false);
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

	// Critical section: writing to the parent connection.
	r = pthread_rwlock_wrlock(&parent_conn_rwlock);
	if (r != 0) cb_eperror(r);
	// Parent connection dead, now we are the root.
	conn_destroy(parent_conn);
	root = true; // TODO: data race here
	// with one server and one child server
	/*
	==================
WARNING: ThreadSanitizer: data race (pid=7935)
  Write of size 1 at 0x55decf2e1d68 by thread T1 (mutexes: write M3):
    #0 cleanup_serve_parent /home/pineman/code/proj/psis/clipboard/parent.c:117:7 (clipboard+0xc7809)
    #1 serve_parent /home/pineman/code/proj/psis/clipboard/parent.c:71:2 (clipboard+0xc75f0)
    #2 cb_recv /home/pineman/code/proj/psis/cb_common/cb_msg.c:86:11 (clipboard+0xc46cf)
    #3 cb_recv_msg /home/pineman/code/proj/psis/cb_common/cb_msg.c:161:6 (clipboard+0xc48ef)
    #4 serve_parent /home/pineman/code/proj/psis/clipboard/parent.c:75:7 (clipboard+0xc7118)

  Previous read of size 1 at 0x55decf2e1d68 by main thread:
    #0 main_cleanup /home/pineman/code/proj/psis/clipboard/main.c:153:7 (clipboard+0xcbde4)
    #1 main /home/pineman/code/proj/psis/clipboard/main.c:211:2 (clipboard+0xcc1c1)

  Location is global 'root' of size 1 at 0x55decf2e1d68 (clipboard+0x000000b2fd68)

  Mutex M3 (0x55decf2e1d30) created at:
    #0 pthread_rwlock_init <null> (clipboard+0x6a2a4)
    #1 init_globals /home/pineman/code/proj/psis/clipboard/main.c:59:6 (clipboard+0xcb1ec)
    #2 main /home/pineman/code/proj/psis/clipboard/main.c:167:2 (clipboard+0xcbff8)

  Thread T1 (tid=7938, running) created by main thread at:
    #0 pthread_create <null> (clipboard+0x54207)
    #1 create_threads /home/pineman/code/proj/psis/clipboard/main.c:122:7 (clipboard+0xcb882)
    #2 listen_sockets /home/pineman/code/proj/psis/clipboard/main.c:102:2 (clipboard+0xcb534)
    #3 main /home/pineman/code/proj/psis/clipboard/main.c:191:2 (clipboard+0xcc038)

SUMMARY: ThreadSanitizer: data race /home/pineman/code/proj/psis/clipboard/parent.c:117:7 in cleanup_serve_parent
==================
*/
	parent_conn = NULL;
	r = pthread_rwlock_unlock(&parent_conn_rwlock);
	if (r != 0) cb_eperror(r);
}
