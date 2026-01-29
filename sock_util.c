#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "sock_util.h"

ssize_t send_all(int sockfd, void *buf, size_t len, int flags)
{
	size_t total_sent = 0;

	while (total_sent < len) {
		ssize_t sent = send(sockfd, (char *)buf + total_sent,
				    len - total_sent, flags);

		if (sent <= 0) {
			if (errno == EINTR || errno == EAGAIN ||
			    errno == EWOULDBLOCK) {
				continue;
			}

			return -1;
		}

		total_sent += sent;
	}

	return total_sent;
}

ssize_t recv_all(int sockfd, void *buf, size_t len, int flags)
{
	size_t total_recvd = 0;

	while (total_recvd < len) {
		ssize_t recvd = recv(sockfd, (char *)buf + total_recvd,
				     len - total_recvd, flags);

		if (recvd == 0) {
			return total_recvd;
		}

		if (recvd < 0) {
			if (errno == EINTR || errno == EAGAIN ||
			    errno == EWOULDBLOCK) {
				continue;
			}

			return -1;
		}

		total_recvd += recvd;
	}

	return total_recvd;
}

mpkt_t *mpkt_new(void *data, uint32_t len)
{
	mpkt_t *mpkt = malloc(sizeof(mpkt_t) + len);

	mpkt->len = len;
	if (data) {
		memcpy(mpkt->data, data, len);
	} else {
		memset(mpkt->data, 0, len);
	}

	return mpkt;
}

void mpkt_free(mpkt_t *mpkt)
{
	if (mpkt) {
		free(mpkt);
	}
}

int mpkt_send(int sockfd, mpkt_t *mpkt, int flags)
{
	uint32_t data_len = htonl(mpkt->len);

	if (send_all(sockfd, &data_len, sizeof(data_len), flags) < 0)
		return -1;
	if (send_all(sockfd, mpkt->data, mpkt->len, flags) < 0)
		return -1;

	return 0;
}

mpkt_t *mpkt_recv(int sockfd, int flags)
{
	uint32_t data_len;

	ssize_t res = recv_all(sockfd, &data_len, sizeof(data_len), flags);
	if (res <= 0) {
		return res == 0 ? (mpkt_t *)-1 : NULL;
	}

	data_len = ntohl(data_len);

	mpkt_t *mpkt = malloc(sizeof(mpkt_t) + data_len);
	if (!mpkt) {
		return NULL;
	}

	mpkt->len = data_len;

	res = recv_all(sockfd, mpkt->data, data_len, flags);

	if (res <= 0) {
		return res == 0 ? (mpkt_t *)-1 : NULL;
	}

	return mpkt;
}