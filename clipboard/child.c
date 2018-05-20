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

	pthread_cleanup_push(cleanup_child_accept, &child_socket);

	while (1) {
		int child = accept(child_socket, NULL, 0);
		if (child == -1) goto cont;

		struct conn *conn = NULL;
		r = conn_new(&conn, child);
		if (r != 0) goto cont_conn;

		r = pthread_create(&conn->tid, NULL, serve_child, conn);
		if (r != 0) goto cont_conn;

		r = conn_append(child_conn_list, conn, &child_conn_list_rwlock);
		if (r != 0) cb_eperror(r);
		continue;

	cont_conn:
		conn_destroy(conn);
	cont:
		cb_perror(errno);
	}

	pthread_cleanup_pop(1);
}

void cleanup_child_accept(void *arg)
{
	int r;
	int *inet_socket = (int *) arg;
	struct conn *conn;
	pthread_t t;

	cb_log("%s", "cleaning up child_accept\n");

	// Cancel all child threads (always fetch next of head)
	while (1) {
		r = pthread_rwlock_rdlock(&child_conn_list_rwlock);
		if (r != 0) cb_eperror(r);
		conn = child_conn_list->next;
		r = pthread_rwlock_unlock(&child_conn_list_rwlock);
		if (r != 0) cb_eperror(r);
		if (conn == NULL) break;
		t = conn->tid;

		r = pthread_cancel(t);
		if (r != 0) cb_eperror(r);
		r = pthread_join(t, NULL);
		if (r != 0) cb_eperror(r);
	}

	// Close inet socket
	close(*inet_socket);
}

void *serve_child(void *arg)
{
	int r;
	struct conn *conn = (struct conn *) arg;

	uint8_t cmd = 0;
	uint8_t region = 0;
	uint32_t data_size = 0;
	char *data = NULL;

	struct clean clean = {.data = &data, .conn = conn};
	pthread_cleanup_push(cleanup_serve_child, &clean);

	while (1)
	{
		r = cb_recv_msg(conn->sockfd, &cmd, &region, &data_size);
		if (r == 0) { cb_log("%s", "child disconnect\n"); break; } // TODO
		if (r == -1) { cb_log("recv_msg failed r = %d, errno = %d\n", r, errno); break; } // TODO
		if (r == -2) { cb_log("recv_msg got invalid message r = %d, errno = %d\n", r, errno); break; } // TODO
		cb_log("[GOT] cmd = %d, region = %d, data_size = %d\n", cmd, region, data_size);

		// Can only receive copy from children. Terminate connection if not
		if (cmd != CB_CMD_COPY) {
			break;
		}

		do_copy(conn->sockfd, region, data_size, &data);
	}

	// Make thread non-joinable: accept loop cleanup will not join() us because
	// we need to die right now (our connection died).
	r = pthread_detach(pthread_self());
	if (r != 0) cb_perror(r);

	pthread_cleanup_pop(1);

	return NULL;
}

// Free temporary data buffer & connection and remove connection from global list.
void cleanup_serve_child(void *arg)
{
	int r;
	struct clean *clean = (struct clean *) arg;
	if (*clean->data != NULL) free(*clean->data);

	r = conn_remove(clean->conn, &child_conn_list_rwlock);
	if (r != 0) cb_eperror(r);

	conn_destroy(clean->conn);
}

