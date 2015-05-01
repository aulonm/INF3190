LC_ALL=C
LANG=C
CC=gcc
CFLAGS=-Wall

.PHONY: clean all

all: server client mip-daemon miptp

client: client.c

server: server.c

mip-daemon: mip-daemon.c

miptp: miptp.c

clean:
	rm -f server
	rm -f client
	rm -f mip-daemon
	rm -f miptp

