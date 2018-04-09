# Roadmap
 * Start by local connections, 1 to 1
 * Leave clipboard server replication for later (single mode first)

# Apps (clients)
 * Apps may connect to more than one local clipboard server, by the server's UDS, whose path is passed to `clipboard_connect`.
 * Apps identify a clipboard by `int clipboard_id`, returned by `clipboard_connect`.
 * Each connection to a clipboard is handled by the library.

# API Library (interface)
 * Write `void *buf` from app to server socket (`clipboard_copy`).
 * Read into the app's `void *buf` from server socket (`clipboard_paste`).
 * These two write/read follow a protocol between the library and the local clipboard server.
 * Connect to the local clipboard server by UDS (or shared memory (unrelated processes) if `void *buf` too big - Performance optimization the prof. suggested).
 * `SOCK_STREAM`

# Clipboard (server)
 * When starting, connect to another clipboard server given in the argv[].
 * Synchronize our local regions with the other remote servers.
 * Each local clipboard should have one thread that receives connections from local applications.
 * Each local clipboard should have one thread that receives connections from cooperative remote clipboards.
 * Each local clipboard should create one thread for every connected applications.
 * Regions will be protected by a lock.

### Protocol
 * Between library and clipboard server
 * Message is: <command><region><size><data>
 * `command` is 8 bits `uint8_t`
 * `region` is 8 bits `uint8_t`
 * `size` is 32 bits `uint32_t`
 * `data` is whatever `size`

### TODO
 * Remember to handle signals (probably)
