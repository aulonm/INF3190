#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <time.h>


int main(int argc, char* argv[]){

	// To check if there are enough arguments
	if(argc != 4){
		printf("USAGE: %s [socket] [address] [message]\n", argv[0]);
		printf("Socket: Socket you want to connect to\n");
		printf("Address: Where to send the message\n");
		printf("Message: The message you want to send\n");
		return -1;
	}

	/* Set the respective variables */
	clock_t begin, end;
	double time_spent;
	fd_set readfds;
	struct timeval tv;
	const char *sockname = argv[1];
	const char *addr = argv[2];
	const char *message	= argv[3];
	
	uint8_t addrs = strtoul(addr, 0, 10);

	/**
	*   AF_UNIX         =	Local communication
	*   SOCK_SEQPACKET  =	Reliable Two-way connection
	*   0               =
	**/
	int usock = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	/* If the clientsocket returns -1 there is a problem */
	if(usock == -1){
		perror("socket");
		return -2;
	}

	/* Bind the socket to the specified interface */
	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));

	/* Connect the socket and check if it worked */
	if(connect(usock, (struct sockaddr*)&bindaddr, sizeof(bindaddr))){
		perror("Problems with connect");
		return -3;
	}

	char buf[1500];
	memset(buf, 0, sizeof(buf));
	buf[0] = addrs; 									/* sets the addr at the first place */
	buf[1] = 0;
	strcat(buf, message);
	printf("buf: %s\n", buf+1);

	/* Set up select */
	FD_ZERO(&readfds);
	FD_SET(usock, &readfds);



	/* start the timer */
	begin = clock();
	tv.tv_sec = 1;

	printf("%u\n", buf[0]);

	ssize_t sent = send(usock, buf, strlen(buf), 0);

	if(sent < 0){
		perror("Send");
		return -4;
	}

	int rv = select(usock+1, &readfds, NULL, NULL, &tv);

	if(rv == 0){
		printf("TIMEOUT!");
		return -5;
	}

	memset(buf, 0, sizeof(buf));
	ssize_t recvd = recv(usock, buf, sizeof(buf), 0);
	
	if(recvd < 0){
		perror("read");
		return -6;
	}

	buf[recvd] = 0;

	/* stop timer */
	end = clock();
	time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
	printf("Received message: %.*s\n", recvd-1, buf+1);
	printf("Time used: %f\n", time_spent);
	close(usock);
	return 0;
}