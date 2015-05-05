LC_ALL=C
LANG=C
CC=gcc
CFLAGS=-Wall

.PHONY: clean all

all: server client mip-daemon miptp file_client file_server

client: client.c

server: server.c

mip-daemon: mip-daemon.c

miptp: miptp.c

file_client: file_client.c

file_server: file_server.c

clean:
	rm -f server
	rm -f client
	rm -f mip-daemon
	rm -f miptp
	rm -f file_client
	rm -f file_server

