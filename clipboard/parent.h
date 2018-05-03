#include <arpa/inet.h>

void validate_addr(char *addr, char *port, struct sockaddr_in *sockaddr);
int connect_parent(char *addr, char *port);
void *serve_parent(void *arg);
