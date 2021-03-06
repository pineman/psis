#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

/*
 * Message is: <command><region><size>
 * `command` is 1 byte `uint8_t`
 * `region` is 1 byte `uint8_t`
 * `data_size` is 4 bytes `uint32_t`
 */
#define CB_MSG_SIZE (1+1+4)
enum cb_cmd {
	CB_CMD_COPY,
	CB_CMD_RESP_COPY_OK,
	CB_CMD_RESP_COPY_NOK,
	CB_CMD_REQ_PASTE,
	CB_CMD_REQ_WAIT,
	CB_CMD_PASTE,
	CB_NUM_CMDS // Number of commands, keep at the end
};

int cb_setsockopt(int sockfd);
// Wrappers
ssize_t cb_send(int sockfd, void *buf, size_t len);
ssize_t cb_recv(int sockfd, void *buf, size_t len);
// Send and receive messages
ssize_t cb_send_msg(int clipboard_id, uint8_t cmd, uint8_t region, uint32_t data_size);
ssize_t cb_recv_msg(int clipboard_id, uint8_t *cmd, uint8_t *region, uint32_t *data_size);
bool cb_send_msg_data(int sockfd, uint8_t cmd, uint8_t region, uint32_t data_size, char *data);
