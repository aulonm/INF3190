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
	

	struct addrinfo hints;
	struct addrinfo *res = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNIX;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;


	//strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));

	int retv = getaddrinfo(sockname, 80, &hints, &res);
	if(retv != 0){
		printf("getaddrinfo failed: %s\n", gai_strerror(retv));
		return -2;
	}

	struct addrinfo *cur = res;

	int usock = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);

	if(usock == -1){
		perror("socket");
		return -3;
	}

	retv = connect(usock, cur->ai_addr, cur->ai_addrlen);

	if(retv != 0){
		perror("Problems with connect");
		return -4;
	}

	freeaddrinfo(res);

	char buf[100];
	
	strcpy(buf, message);
	ssize_t sent = send(usock, buf, strlen(buf), 0);
	if(sent <= 0 ){
		perror("Problems with send");
		return -5;
	}

	ssize_t recvd = recv(usock, buf, sizeof(buf), 0);

	if(recvd <= 0){
		perror("Problems with read");
		return -6;
	}

	buf[recvd] = '\0';
	printf("Received message: %s\n", buf);

	close(usock);
	return 0;

}