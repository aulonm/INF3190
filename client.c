#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>


int main(int argc, char* argv[]){

	// To check if there are enough arguments
	if(argc != 4){
		printf("USAGE: %s [domain name] [address] [message]\n", argv[0]);
		printf("");
		printf("");
		return -1;
	}

	const char *sockname = argv[1];
	const char *addr = argv[2];
	const char *message	= argv[3];

	uint8_t addrs = strtoul(addr, 0, 10);
	printf("%u\n", addrs);

	//strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));

	int usock = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	if(usock == -1){
		perror("socket");
		return -2;
	}

	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));

	if(connect(usock, (struct sockaddr*)&bindaddr, sizeof(bindaddr))){
		perror("Problems with connect");
		return -3;
	}

	char buf[100];
	buf[0] = addrs;
	buf[1] = 0;
	//strcpy(buf, (char)addrs);
	strcat(buf, message);
	printf("buf: %s\n", buf+1);
	ssize_t sent = send(usock, buf, sizeof(buf), 0);

	ssize_t recvd = recv(usock, buf, 99, 0);
	if(recvd < 0){
		perror("read");
		return -4;
	}

	buf[recvd] = 0;
	printf("Received message: %.*s\n", recvd-1, buf+1);


	close(usock);
	return 0;

}