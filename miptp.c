#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>

#include <sys/ioctl.h>
#include <bits/ioctls.h>


struct tp_packet {
	unsigned int PL: 2;
	unsigned int port: 14;
	unsigned int seqnr: 16;
} __attribute__((packed));




int main(int argc, char *argv[]){
	int clientSocket, mipSocket;
	const char *sock1, *sock2;

	if(argc < 4){
		printf("USAGE: %s [sock1] [sock2] [-d]\n", argv[0]);
		printf("Sock1: Socket to connecto to\n");
		printf("Sock2: Socket to connect from\n");
	}

	sock1 = argv[1];
	sock2 = argv[2];

	clientSocket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	mipSocket = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	if(clientSocket == -1){
		perror("ClientSocket");
		return -1;
	}

	if(mipSocket == -1){
		perror("mipSocket");
		return -1;
	}

	// Bind the AF_UNIX socket
	struct sockaddr_un bindaddrClient;
	bindaddrClient.sun_family = AF_UNIX;
	strncpy(bindaddrClient.sun_path, sock1, sizeof(bindaddrClient.sun_path));
	int retv = bind(clientSocket, (struct sockaddr *)&bindaddrClient, sizeof(bindaddrClient.sun_path));

	/* If it doesnt return 0, it couldnt bind to the socket */
	if(retv != 0){
		perror("bind client");
		return -1;
	}

	
	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sock2, sizeof(bindaddr.sun_path));

	if(connect(mipSocket, (struct sockaddr*)&bindaddr, sizeof(bindaddr))){
		perror("Connect");
		return -1;
	}


	if(listen(clientSocket, 5) != 0){
		perror("listen");
		return -1;
	}

	int clientfds[10];
	memset(clientfds, 0, sizeof(clientfds));


	close(clientSocket);
	close(mipSocket);
	return 0;
}