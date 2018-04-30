#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../common.h"

// Stores size of the message in *msg_size
// Returns a buffer containing the message
// Caller must free return value.
// if data_size == 0, data is ignored.
uint8_t *make_msg(uint8_t cmd, uint8_t region, uint32_t data_size, void *data)
{
	int i = 0; // track buf position
	uint8_t *msg_buf = malloc(MSG_HEADER_SIZE + MSG_DATA_MAX_SIZE);

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

	return msg_buf;
}

// Returns number of user data bytes written (not including the header)
// or 0 on error
int send_msg(int clipboard_id, uint32_t data_size, uint8_t *msg)
{
	int r, ret;
	int msg_size = MSG_HEADER_SIZE + data_size;

	r = send(clipboard_id, msg, msg_size, 0);
	if (r == -1) {
		if (errno == EBADF || errno == ECONNRESET || errno == EACCES || errno == EPIPE)
			// Return zero when there's a connection error
			ret = 0;
		else
			eperror(errno);
	}
	else if (r == msg_size) {
		// Success: write() returned the number of bytes we asked for
		// return number of user data bytes (all of them were written)
		ret = data_size;
	}
	else {
		// write() returned less bytes than we asked for
		// return number of user data bytes actually written, if any
		ret = r - MSG_HEADER_SIZE;
		if (ret < 0) ret = 0;
	}

	return ret;
}

// Stores size of the message in *msg_size
// Returns a buffer containing the message
// Caller must free return value.
uint8_t *recv_msg(int clipboard_id, int *msg_size)
{
	int r;

	uint8_t *msg_buf = malloc(MSG_HEADER_SIZE + MSG_DATA_MAX_SIZE);
	r = recv(clipboard_id, (void *) msg_buf, MSG_HEADER_SIZE, 0);

	int data_size;
	memcpy(&data_size, (void *) (msg_buf+1+1), 4);

	r += recv(clipboard_id, (void *) (msg_buf+MSG_HEADER_SIZE), data_size, 0);

	*msg_size = r;

	return msg_buf;
}

