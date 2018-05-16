#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Message is: <command><region><size>
 * `command` is 1 byte `uint8_t`
 * `region` is 1 byte `uint8_t`
 * `data_size` is 4 bytes `uint32_t`
 */
#define CB_MSG_SIZE 1+1+4
enum cb_cmd {
	CB_CMD_COPY,
	CB_CMD_REQ_PASTE,
	CB_CMD_PASTE,
	CB_NUM_CMDS // Number of commands, keep at the end
};
#define CB_REQ_PASTE_DATA "req_paste"
#define CB_REQ_PASTE_DATA_SIZE 10

int erecv(int sock_fd, void *buf, size_t len);
int esend(int sock_fd, void *buf, size_t len);

int send_msg(int clipboard_id, uint8_t cmd, uint8_t region, uint32_t data_size);
int recv_msg(int clipboard_id, uint8_t *cmd, uint8_t *region, uint32_t *data_size);
