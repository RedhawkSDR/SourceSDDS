#ifndef MULTICAST_H_
#define MULTICAST_H_

#include <arpa/inet.h>
#include <stdexcept>
#include "SourceNicUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int sock;
  struct sockaddr_in addr;
} multicast_t;

multicast_t multicast_client (const char* iface, const char* group, int port) throw (BadParameterError);
ssize_t multicast_receive (multicast_t client, void* buffer, size_t bytes);
multicast_t multicast_server (const char* iface, const char* group, int port);
ssize_t multicast_transmit (multicast_t server, const void* buffer, size_t bytes);
int multicast_poll_in (multicast_t client, int timeout);
void multicast_close(multicast_t socket);

#ifdef __cplusplus
}
#endif

#endif /* MULTICAST_H_ */
