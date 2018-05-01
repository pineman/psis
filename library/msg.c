#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../common.h"

// Modify a buffer msg_buf to become a message with given arguments.
// if data_size == 0, data is ignored.
void make_msg(uint8_t *msg_buf, uint8_t cmd, uint8_t region, uint32_t data_size, void *data)
{
	int i = 0; // track buf position

	// command (1 byte)
	msg_buf[i] = cmd;
	i += 1;

	// region (1 byte)
	msg_buf[i] = region;
	i += 1;

	// data_size (4 bytes)
	memcpy(msg_buf+i, (void *) &data_size, 4);
	i += 4;

	// Skip data for messages without data (data_size = 0)
	if (data_size != 0) {
		// data (`data_size` bytes)
		memcpy(msg_buf+i, data, data_size);
	}
}

// Send message to a clipboard server
// Returns number of user data bytes written (not including the header)
// or 0 on error
int send_msg(int clipboard_id, uint32_t data_size, uint8_t *msg)
{
	int r, ret;
	int msg_size = CB_HEADER_SIZE + data_size;

	r = send(clipboard_id, msg, msg_size, 0);
	if (r == -1) {
		if (errno == EBADF || errno == ECONNRESET || errno == EACCES || errno == EPIPE)
			// Return zero when there's a connection error
			ret = 0;
		else
			eperror(errno);
	}
	else if (r == msg_size) {
		// Success: send() returned the number of bytes we asked for
		// return number of user data bytes (all of them were written)
		ret = data_size;
	}
	else {
		// send() returned less bytes than we asked for
		// return number of user data bytes actually written, if any
		ret = r - CB_HEADER_SIZE;
		if (ret < 0) ret = 0;
	}

	return ret;
}

// Read and parse msg_buf to the arguments given by reference.
// if data_size == 0, data is ignored.
void parse_msg(uint8_t *msg_buf, uint8_t *cmd, uint8_t *region, uint32_t *data_size, void **data)
{
	int i = 0;

	// command (1 byte)
	*cmd = msg_buf[i];
	i++;

	// region (1 byte)
	*region = msg_buf[i];
	i++;

	// data_size (4 bytes)
	*data_size = *((uint32_t *) msg_buf+i);
	i += 4;

	// Skip data for messages without data (data_size = 0)
	if (data_size != 0) {
		*data = msg_buf+i;
	}
}

// Wrapper around recv()
// Returns number of bytes read from clipboard or 0 on error.
int erecv(int sock_fd, void *buf, size_t len)
{
	int r;

	r = recv(sock_fd, buf, len, MSG_WAITALL);
	if (r == -1) {
		if (errno == EBADF || errno == ECONNRESET || errno == EACCES || errno == EPIPE)
			// Return zero when there's a connection error
			return 0;
		else
			eperror(errno);
	}

	return r;
}

// Receive a message from the clipboard
// Returns number of clipboard data bytes read
int recv_msg(int clipboard_id, uint8_t *msg_buf)
{
	int r;

	// Get the message header
	r = erecv(clipboard_id, (void *) msg_buf, CB_HEADER_SIZE);
	if (r == 0) {
		return 0;
	}

	// Get data_size from the header (1+1 bytes up)
	size_t data_size = *((size_t *) (msg_buf+1+1));
	// Get clipboard data
	r = erecv(clipboard_id, (void *) msg_buf + CB_HEADER_SIZE, data_size);
	if (r == 0) {
		return 0;
	}
	// Return number of clipboard data bytes
	return r;
}
