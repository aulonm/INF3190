#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>


int main(int argc, char* argv[]){

	// To check if there are enough arguments
	if(argc != 3){
		printf("USAGE: %s [domain name] [message]\n", argv[0]);
		return -1;
	}

	const char *sockname = argv[1];
	const char *message = argv[2];



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
	strcpy(buf, message);
	printf("buf: %s\n", buf);
	ssize_t sent = send(usock, buf, sizeof(buf), 0);

	ssize_t recvd = recv(usock, buf, 99, 0);
	if(recvd < 0){
		perror("read");
		return -4;
	}

	buf[recvd] = 0;
	printf("Received message: %.*s\n", recvd, buf);


	close(usock);
	return 0;

}