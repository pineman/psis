# Apps (clients)
 * Apps may connect to more than one local clipboard server. But only local ones, i.e., by UDS, whose path is passed in `clipboard_connect`.
 * Apps identify a clipboard by `clipboard_id`, returned by `clipboard_connect`.
 * Each connection to a clipboard is handled by the library.

# API Library (interface)
 * Handle connections to clipboard servers.
 * Copy `void *buf` from app to the server.
 * Connect to the clipboard server by UDS; or shared memory (unrelated processes) if `void *buf` too big.
 * Define protocol between library interface and clipboard server.

# Clipboard (server)
 * When starting, connect to another clipboard server given in the argv[].
 * Synchronize our local regions with the other remote servers.
 * Each local clipboard should have one thread that receives connections from local applications.
 * Each local clipboard should have one thread that receives connections from cooperative remote clipboards.
 * Each local clipboard should create one thread for every connected applications.
 * Regions will be protected by a lock.
