LIB_HEADERS = list.h sock_util.h
LIB_OBJS = list.o sock_util.o
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=gnu99 -g

all: server client

server: server.o $(LIB_OBJS)
	$(CC) -o $@ $^

client: client.o $(LIB_OBJS)
	$(CC) -o $@ $^

%.o: %.c $(LIB_HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -rf server client *.o

.PHONY: all clean
