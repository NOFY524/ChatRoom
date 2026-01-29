#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "sock_util.h"

#define DEFAULT_PORT 50204
#define MAX_NAME_LEN 32

struct server {
	int sockfd;
	struct sockaddr_in address;
};

struct server server;

void *recv_thread(void *arg)
{
	(void)arg;

	while (1) {
		mpkt_t *mpkt = mpkt_recv(server.sockfd, 0);

		if (mpkt == (mpkt_t *)-1) {
			printf("Disconnected from server\n");

			return NULL;
		} else if (!mpkt) {
			perror("Receiving from server failed");

			mpkt_free(mpkt);

			return NULL;
		}

		printf("%s\n", (char *)mpkt->data);
		mpkt_free(mpkt);
	}

	return NULL;
}

void *send_thread(void *arg)
{
	(void)arg;

	char buffer[1024];

	while (1) {
		if (!fgets(buffer, sizeof(buffer), stdin)) {
			printf("stdin closed\n");
			break;
		}

		buffer[strcspn(buffer, "\n")] = 0;

		if (!strcmp(buffer, "exit")) {
			return NULL;
		}

		mpkt_t *mpkt = mpkt_new(buffer, strlen(buffer) + 1);
		if (mpkt_send(server.sockfd, mpkt, 0) == -1) {
			perror("sending message failed");
			mpkt_free(mpkt);
			break;
		}
		mpkt_free(mpkt);
	}

	return NULL;
}

int main(int argc, char **argv)
{
	server.sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (server.sockfd == -1) {
		perror("Opening socket failed");
		exit(1);
	}

	uint16_t port = DEFAULT_PORT;
	char *address = NULL;
	int port_selected = 0;
	int address_selected = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-p") == 0) {
			if (port_selected) {
				fprintf(stderr,
					"multiple -p options detected\n");
				exit(1);
			}
			if (i + 1 >= argc) {
				fprintf(stderr, "-p requires a port number\n");
				exit(1);
			}
			port = (uint16_t)atoi(argv[++i]);
			port_selected = 1;
		} else {
			if (address_selected) {
				fprintf(stderr, "multiple address detected\n");

				exit(1);
			}

			address = argv[i];
			address_selected = 1;
		}
	}

	if (!address) {
		address = "127.0.0.1";
	}

	memset(&server.address, 0, sizeof(server.address));
	server.address.sin_family = AF_INET;
	server.address.sin_port = htons(port);

	if (inet_pton(AF_INET, address, &server.address.sin_addr) <= 0) {
		perror("Invalid address");
		exit(1);
	}

	if (connect(server.sockfd, (struct sockaddr *)&server.address,
		    sizeof(server.address)) < 0) {
		perror("connect failed");
		exit(1);
	}

	mpkt_t *mpkt = mpkt_recv(server.sockfd, 0);
	if (!mpkt) {
		perror("receiving greeting failed");
		close(server.sockfd);
		exit(1);
	}

	printf("%s", (char *)mpkt->data);
	fflush(stdout);
	mpkt_free(mpkt);

	char name_buffer[MAX_NAME_LEN];
	if (!fgets(name_buffer, sizeof(name_buffer), stdin)) {
		fprintf(stderr, "getting input failed\n");
		close(server.sockfd);
		exit(1);
	}

	name_buffer[strcspn(name_buffer, "\n")] = 0;

	mpkt = mpkt_new(name_buffer, strlen(name_buffer) + 1);
	if (mpkt_send(server.sockfd, mpkt, 0) == -1) {
		perror("sending name failed");
		mpkt_free(mpkt);
		close(server.sockfd);
		exit(1);
	}
	mpkt_free(mpkt);

	pthread_t recv_t, send_t;
	pthread_create(&recv_t, NULL, recv_thread, NULL);
	pthread_create(&send_t, NULL, send_thread, NULL);

	pthread_join(send_t, NULL);
	shutdown(server.sockfd, SHUT_RDWR);
	pthread_join(recv_t, NULL);

	close(server.sockfd);
	return 0;
}
