CC=gcc
CFLAGS=-Wall

.PHONY: clean all

all: server client

client: client.c

server: server.c

clean:
	rm -f server
	rm -f client