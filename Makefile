CC=gcc
CFLAGS=-Wall

.PHONY: clean all

all: server client mip-daemon

client: client.c

server: server.c

mip-daemon: mip-daemon.c

clean:
	rm -f server
	rm -f client
	rm -f mip-daemon
