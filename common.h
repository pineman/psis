/*
 * Message is: <command><region><size><data> == <header><data>
 * `command` is 1 byte `uint8_t`
 * `region` is 1 byte `uint8_t`
 * `data_size` is 4 bytes `uint32_t`
 * `data` is whatever `size`
 */

enum cb_cmd {CB_CMD_COPY, CB_CMD_REQ_PASTE, CB_CMD_PASTE};
#define CB_HEADER_SIZE 1 + 1 + 4
#define CB_DATA_MAX_SIZE 20

#define CB_SOCKET "/cb_sock"
#define CB_NUM_REGIONS 10

#define eperror(r) { fprintf(stderr, "%s:%d errno = %d: %s\n", __FILE__, __LINE__-1, r, strerror(r)); exit(r); }
