#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/socket.h>

#include "cb_common.h"
#include "cb_msg.h"

// Set socket option NOSIGPIPE, if available (macOS, etc.)
// return value is:
//	-1 on error and errno is set
//	0 on success
int cb_setsockopt(int sockfd)
{
	(void) sockfd;

#ifdef SO_NOSIGPIPE
	//	Make sure that SIGPIPE signal is not generated when writing to a
	//	connection that was already closed by the peer.
	int set = 1;
	r = setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(int));
	if (r == -1) return -1;
#endif

	return 0;
}

// Wrapper around send(), write len bytes to sockfd (inspired by Stevens book)
// len must not be 0
// return value is:
//	-1 on error and errno is set
//	`len` on success
ssize_t cb_send(int sockfd, void *buf, size_t len)
{
	assert(len != 0);

	int flags = 0;
#ifdef MSG_NOSIGNAL
	flags = MSG_NOSIGNAL; // Do not receive SIGPIPE
#endif

	size_t nleft = len;
	char *ptr = (char *) buf;
	ssize_t nwritten = 0;
	while (nleft > 0) {
		nwritten = send(sockfd, ptr, nleft, flags);
		// Ignore if write returns 0. Hopefully its transient and, if not,
		// we will eventually get nwritten = -1 indicating an error.
		if (nwritten == -1) {
			if (errno == EINTR) continue;
			else return -1;
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
	return len;
}

// Wrapper around recv(), read len bytes from sockfd (inspired by Stevens book)
// len must not be 0
// return value is:
//	-1 on error and errno is set
//	0 on EOF (buf may have some written bytes)
//	`len` on success
ssize_t cb_recv(int sockfd, void *buf, size_t len)
{
	assert(len != 0);

	int flags = 0;

	size_t nleft = len;
	char *ptr = (char *) buf;
	ssize_t nread = 0;
	while (nleft > 0) {
		nread = recv(sockfd, ptr, nleft, flags);
		if (nread == -1) {
			if (errno == EINTR) continue;
			else return -1;
		}
		// EOF, return 0
		if (nread == 0) return 0;
		nleft -= nread;
		ptr += nread;
	}
	if ((size_t) nread < len) return 0;
	return len;
}

// Check if message is valid
// return true or false
static bool valid_msg(uint8_t cmd, uint8_t region)
{
	// cmd must be valid
	if (cmd >= CB_NUM_CMDS) {
		return false;
	}

	// region must be in 0..CB_NUM_REGIONS-1
	if (region > CB_NUM_REGIONS-1) {
		return false;
	}

	return true;
}

// Send message to a clipboard server
// return value is:
//	-2 in case the message requested to be sent is invalid
//	-1 on error and errno is set
//	CB_MSG_SIZE on success
ssize_t cb_send_msg(int clipboard_id, uint8_t cmd, uint8_t region, uint32_t data_size)
{
	ssize_t r;

	// Return -2 early if message is invalid
	if (!valid_msg(cmd, region)) return -2;

	uint8_t msg[CB_MSG_SIZE];
	// command (1 byte)
	msg[0] = cmd;
	// region (1 byte)
	msg[1] = region;
	// data_size (4 bytes)
	memcpy(msg+2, (void *) &data_size, 4);

	// Send message
	r = cb_send(clipboard_id, (void *) msg, CB_MSG_SIZE);
	// Error
	if (r == -1) return -1;
	// Success
	return r;
}

// Receive a message from the clipboard
// return value is:
//	-2 in case the message to received is invalid (and nothing is copied)
//	-1 on error and errno is set
//	0 when clipboard_fd was disconnected during recv() (EOF)
//	CB_MSG_SIZE on success
ssize_t cb_recv_msg(int clipboard_id, uint8_t *cmd, uint8_t *region, uint32_t *data_size)
{
	ssize_t r;

	// Fatal error, can't copy to supplied pointers
	assert(cmd != NULL);
	assert(region != NULL);
	assert(data_size != NULL);

	uint8_t msg[CB_MSG_SIZE];
	// Get the message
	r = cb_recv(clipboard_id, (void *) msg, CB_MSG_SIZE);
	// Return -1 early on error or 0 on EOF
	if (r == -1 || r == 0) return r;

	// Return -2 early if message is invalid
	if (!valid_msg(msg[0], msg[1])) return -2;

	// Success, copy msg to supplied pointers and return
	// command (1 byte)
	*cmd = msg[0];
	// region (1 byte)
	*region = msg[1];
	// data_size (4 bytes)
	*data_size = ((uint32_t) *(msg+1+1));
	return r;
}

bool cb_send_msg_data(int sockfd, uint8_t cmd, uint8_t region, uint32_t data_size, char *data)
{
	ssize_t r;

	r = cb_send_msg(sockfd, cmd, region, data_size);
	if (r == -1) {
		// send_msg failed, terminate connection
		return false;
	}
	// Fatal error: we tried to send an invalid message
	assert(r != -2);

	r = cb_send(sockfd, data, data_size);
	if (r == -1) {
		// send data failed, terminate connection
		return false;
	}

	return true;
}
