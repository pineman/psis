# Roadmap
 * Start by local connections, 1 client to 1 server
 * 1 client to 1 server, local to remote connections (backup lab7)
 * many clients to 1 server, server one thread per client
 * ...
 * Leave clipboard server replication for later (single mode first)

### TODO
 * probably don't use exit() or pthread_exit => Cancel threads and return

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
 * Also need to define a Clipboard Protocol
 * Synchronize our local regions with the other remote servers. 
 * Each local clipboard should have one thread that receives connections from local applications.
 * Each local clipboard should have one thread that receives connections from cooperative remote clipboards.
 * Each local clipboard should create one thread for every connected application.
 * Regions will be protected by a lock.
 * Freitas: only one syncronization thread sends messages to children/parent, other client threads speak to it and enqueue messages ?
 * ~~When we receive updates from remote clipboards, do we need to push them to the app? Or are they in charge of getting new stuff?~~

### Clipboard Protocol
 * Exactly the same as the library protocol?

### Libary protocol
 * Between library and clipboard server
 * Message is: <command><region><size><data>
 * `command` is 1 byte `uint8_t`
 * `region` is 1 byte `uint8_t`
 * `size` is 4 bytes `uint32_t`
 * `data` is whatever `size + 1` (+ 1 for extra \0)
