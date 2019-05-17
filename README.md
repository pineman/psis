Tested on Linux, MacOS, Windows (via MSYS2), OpenIndiana, FreeBSD, OpenBSD.

# Roadmap
 * Start by local connections, 1 client to 1 server
 * 1 client to 1 server, local to remote connections (backup lab7)
 * many clients to 1 server, server one thread per client
 * ...
 * Leave clipboard server replication for later (single mode first)

### TODO
 * on app copy, its `write()` does not wait for the servers `read()` of course. However, the server may fail to allocate enough memory to receive the data, in which case nothing, in practice, was copied to the server's region. So the app should wait for a reply from the server saying it failed, and `clipboard_copy` should return 0.

# Apps (clients)
 * Apps may connect to more than one local clipboard server, by the server's UDS, whose path is passed to `clipboard_connect`.
 * Apps identify a clipboard by `int clipboard_id`, returned by `clipboard_connect`.
 * Each connection to a clipboard is handled by the library.
 * Clients may be multithreaded for the 20, which means the libary has to be thread-safe on _copy requests to the same region. (serialize)

# API Library (interface)
 * Write `void *buf` from app to server socket (`clipboard_copy`).
 * Read into the app's `void *buf` from server socket (`clipboard_paste`).
 * These two write/read follow a protocol between the library and the local clipboard server.
 * Connect to the local clipboard server by UDS (or shared memory (unrelated processes) if `void *buf` too big - Performance optimization the prof. suggested).
 * `SOCK_STREAM`
 * copy/paste: do only one `write` (two `recv`s) to minimize critical region, this forces the use of a msg buffer instead of a struct

# Clipboard (server)
 * When starting, connect to another clipboard server given in the argv[].
 * Synchronize our local regions with the other remote servers. 
 * Each local clipboard should have one thread that receives connections from local applications.
 * Each local clipboard should have one thread that receives connections from cooperative remote clipboards.
 * Each local clipboard should create one thread for every connected application.
 * Regions will be protected by a lock.

### Local protocol
 * Between library and clipboard server
 * Message is: <command><region><size><data>
 * `command` is 1 byte `uint8_t`
 * `region` is 1 byte `uint8_t`
 * `size` is 4 bytes `uint32_t`
 * `data` is whatever `size`
