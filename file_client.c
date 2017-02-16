#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <time.h>

struct info{
	unsigned int port: 14;
}__attribute__((packed));

int main(int argc, char* argv[]){
	FILE *fileptr;
	char *buffer;

	if(argc != 5){
		printf("len: %d\n", argc);
		printf("USAGE: %s [Socket] [filename] [address] [port]\n", argv[0]);
		printf("Socket: Socket you want to connecto to\n");
		printf("Filename: Filename you want to send\n");
		printf("Address: Where to send the file\n");
		printf("Port: Which port\n");
		return -1;
	}

	const char *sockname = argv[1];
	const char *filename = argv[2];
	const char *addr = argv[3];
	unsigned int port = strtoul(argv[4], 0, 10);

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

	//Bind the socket to the specified interface 
	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));

	/* Connect the socket and check if it worked */
	if(connect(usock, (struct sockaddr*)&bindaddr, sizeof(bindaddr))){
		perror("Problems with connect");
		return -3;
	}

	// read file put in buffer
	fileptr = fopen(filename, "r");  
    fseek(fileptr, 0, SEEK_END);  
    ssize_t len = ftell(fileptr);  
    buffer = malloc(len+1);
	buffer[0] = addrs;  
    fseek(fileptr, 0, SEEK_SET);  
    fread(buffer+1, 1, len, fileptr);  
    fclose(fileptr);  

    //info about port to send to tpdaemon
    struct info* info;
    info = malloc(sizeof(struct info));
    info->port = port;

    printf("port %u\n", info->port);

    //Send port
	ssize_t sent = send(usock, info, sizeof(info), 0);

	if(sent < 0){
		perror("send");
		return -1;
	}

	printf("sent 1: %d\n", sent);

	char mipbuf[1];
	mipbuf[0] = addrs;
	int sentCnt = 0;
	// Send 1492 size packets while under the length of the file	
	while(sentCnt < len){
		// Send it
		sent = send(usock, buffer, len+1, 0);	

		if(sent < 0){
			perror("send");
			return -1;
		}
		printf("size %d\n", sent); 
		// Increment the buffer pointer, to get the rest of the file
		buffer = buffer + sent;
		// Increment sentCnt with what is sent
		sentCnt += sent;
	}

	free(info);
	return  0;
}