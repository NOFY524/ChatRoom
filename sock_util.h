#ifndef _SOCK_UTIL_H
#define _SOCK_UTIL_H

#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>

ssize_t send_all(int sockfd, void *buf, size_t len, int flags);
ssize_t recv_all(int sockfd, void *buf, size_t len, int flags);

typedef struct mpkt {
	uint32_t len;
	uint8_t data[];
} mpkt_t;

mpkt_t *mpkt_new(void *data, uint32_t len);
void mpkt_free(mpkt_t *mpkt);

int mpkt_send(int sockfd, mpkt_t *mpkt, int flags);
mpkt_t *mpkt_recv(int sockfd, int flags);

#endif
