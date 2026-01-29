#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stddef.h>
#include <unistd.h>
#include <stdarg.h>

#include "sock_util.h"
#include "list.h"

#define DEFAULT_PORT 50204
#define MAX_NAME_LEN 32

struct client {
	struct list_head list;
	int sockfd;
	struct sockaddr_in address;
	char name[MAX_NAME_LEN];
};

struct message {
	time_t timestamp;
	struct list_head list;
	char sender_name[MAX_NAME_LEN];
	size_t len;
	char data[];
};

struct server {
	int sockfd;
	struct sockaddr_in address;

	struct list_head clients;
	pthread_mutex_t clients_mutex;

	struct list_head messages;
	pthread_mutex_t messages_mutex;
	pthread_cond_t messages_cond;
};

struct server server;

FILE *logfile = NULL;
pthread_mutex_t logfile_mutex;

static void log_event(const char *fmt, ...);

void *broadcast_thread(void *arg);
void *client_handle_thread(void *arg);

int main(int argc, char **argv)
{
	pthread_mutex_init(&server.clients_mutex, NULL);
	pthread_mutex_init(&server.messages_mutex, NULL);
	pthread_cond_init(&server.messages_cond, NULL);
	pthread_mutex_init(&logfile_mutex, NULL);

	logfile = fopen("server.log", "a");
	if (!logfile) {
		perror("logfile opening failed");

		exit(1);
	}

	uint16_t port = DEFAULT_PORT;

	if (argc == 2) {
		port = 0;

		while (*argv[1]) {
			port *= 10;
			port += *argv[1]++ - '0';
		}
	} else if (argc > 2) {
		fprintf(stderr, "multiple port number detected");

		exit(1);
	}

	INIT_LIST_HEAD(&server.clients);
	INIT_LIST_HEAD(&server.messages);

	server.sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (server.sockfd == -1) {
		perror("socket opening failed");

		exit(1);
	}

	server.address.sin_family = AF_INET;
	server.address.sin_addr.s_addr = htonl(INADDR_ANY);
	server.address.sin_port = htons(port);

	if (bind(server.sockfd, (struct sockaddr *)&server.address,
		 sizeof(server.address))) {
		perror("socket binding failed");

		exit(1);
	}

	listen(server.sockfd, 5);

	pthread_t tid;

	pthread_create(&tid, NULL, broadcast_thread, "server.log");

	pthread_detach(tid);

	while (1) {
		struct client *client = malloc(sizeof(struct client));

		if (!client) {
			perror("allocating memory for client failed");

			exit(1);
		}

		memset(client, 0, sizeof(struct client));

		socklen_t socklen = sizeof(client->address);
		client->sockfd = accept(server.sockfd,
					(struct sockaddr *)&client->address,
					&socklen);

		if (client->sockfd == -1) {
			perror("accepting client failed");

			free(client);

			continue;
		}

		pthread_create(&tid, NULL, client_handle_thread, client);

		pthread_detach(tid);
	}

	pthread_mutex_destroy(&server.clients_mutex);
	pthread_mutex_destroy(&server.messages_mutex);
}

void log_event(const char *fmt, ...)
{
	time_t t = time(NULL);
	struct tm tm_buf;
	localtime_r(&t, &tm_buf);

	char timestr[64];
	strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm_buf);

	pthread_mutex_lock(&logfile_mutex);
	if (logfile) {
		va_list ap;
		va_start(ap, fmt);
		fprintf(logfile, "[%s] ", timestr);
		vfprintf(logfile, fmt, ap);
		fprintf(logfile, "\n");
		fflush(logfile);
		va_end(ap);
	}
	pthread_mutex_unlock(&logfile_mutex);
}

static void push_system_message(const char *sender, const char *text)
{
	struct message *message =
		malloc(sizeof(struct message) + strlen(text) + 1);
	if (!message)
		return;

	message->timestamp = time(NULL);
	strncpy(message->sender_name, sender, sizeof(message->sender_name));
	message->len = strlen(text) + 1;
	strcpy(message->data, text);

	pthread_mutex_lock(&server.messages_mutex);
	list_add_tail(&message->list, &server.messages);
	pthread_cond_signal(&server.messages_cond);
	pthread_mutex_unlock(&server.messages_mutex);
}

void *broadcast_thread(void *arg)
{
	(void)arg;
	while (1) {
		pthread_mutex_lock(&server.messages_mutex);
		while (list_empty(&server.messages)) {
			pthread_cond_wait(&server.messages_cond,
					  &server.messages_mutex);
		}

		struct list_head *message_list, *message_list_next;
		list_for_each_safe(message_list, message_list_next,
				   &server.messages) {
			struct message *message = container_of(
				message_list, struct message, list);

			struct tm lct;
			localtime_r(&message->timestamp, &lct);
			log_event("%s: %.*s", message->sender_name,
				  (int)message->len, message->data);

			char *message_with_info =
				malloc(strlen(message->sender_name) +
				       message->len + 4);
			sprintf(message_with_info, "[%s]: %s",
				message->sender_name, message->data);

			mpkt_t *mpkt = mpkt_new(message_with_info,
						strlen(message_with_info) + 1);

			free(message_with_info);

			pthread_mutex_lock(&server.clients_mutex);

			struct list_head *client_list;
			list_for_each(client_list, &server.clients) {
				struct client *client = container_of(
					client_list, struct client, list);

				if (mpkt_send(client->sockfd, mpkt, 0) == -1) {
					perror("sending message to client failed");
				}
			}

			pthread_mutex_unlock(&server.clients_mutex);

			mpkt_free(mpkt);

			list_del(message_list);

			free(message);
		}

		pthread_mutex_unlock(&server.messages_mutex);
	}
}

void *client_handle_thread(void *arg)
{
	struct client *client = (struct client *)arg;
	char sysmsg[128];

	char *greeting_message = "Hello! Please select your name: ";
	mpkt_t *mpkt = mpkt_new(greeting_message, strlen(greeting_message) + 1);

	if (mpkt_send(client->sockfd, mpkt, 0) == -1) {
		perror("Sending to client failed");

		goto cleanup;
	}
	mpkt_free(mpkt);

	mpkt = mpkt_recv(client->sockfd, 0);

	if (!mpkt) {
		perror("Receiving from client failed");

		goto cleanup;
	}
	strncpy(client->name, (char *)mpkt->data, sizeof(client->name));
	client->name[MAX_NAME_LEN - 1] = 0;
	mpkt_free(mpkt);

	pthread_mutex_lock(&server.clients_mutex);
	list_add_tail(&client->list, &server.clients);
	pthread_mutex_unlock(&server.clients_mutex);

	printf("%s connected from %s:%d\n", client->name,
	       inet_ntoa(client->address.sin_addr),
	       ntohs(client->address.sin_port));
	fflush(stdout);

	log_event("%s connected from %s:%d", client->name,
		  inet_ntoa(client->address.sin_addr),
		  ntohs(client->address.sin_port));

	snprintf(sysmsg, sizeof(sysmsg), "%s connected", client->name);
	push_system_message("SERVER", sysmsg);

	while (1) {
		mpkt = mpkt_recv(client->sockfd, 0);

		if (mpkt == (mpkt_t *)-1) {
			printf("Client %s has disconnected\n", client->name);
			fflush(stdout);

			snprintf(sysmsg, sizeof(sysmsg), "%s disconnected.",
				 client->name);
			push_system_message("SERVER", sysmsg);

			goto cleanup;
		} else if (!mpkt) {
			perror("Receiving from client failed");

			goto cleanup;
		}

		struct message *message =
			malloc(sizeof(struct message) + mpkt->len);
		message->timestamp = time(NULL);
		strcpy(message->sender_name, client->name);
		message->len = mpkt->len;
		memcpy(message->data, mpkt->data, mpkt->len);

		mpkt_free(mpkt);

		pthread_mutex_lock(&server.messages_mutex);
		list_add_tail(&message->list, &server.messages);
		pthread_cond_signal(&server.messages_cond);
		pthread_mutex_unlock(&server.messages_mutex);
	}
cleanup:
	pthread_mutex_lock(&server.clients_mutex);
	list_del(&client->list);
	pthread_mutex_unlock(&server.clients_mutex);

	close(client->sockfd);

	free(client);

	return NULL;
}
