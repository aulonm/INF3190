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
	unsigned int *port = strtoul(argv[4], 0, 10);

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
    buffer = malloc(len);  
    fseek(fileptr, 0, SEEK_SET);  
    fread(buffer, 1, len, fileptr);  
    fclose(fileptr);  

    //info about port and filesize to send to tpdaemon
    struct info* info;
    info = malloc(sizeof(struct info));
    info->port = port;

    printf("port %u\n", info->port);

    //Send port and filsize
	ssize_t sent = send(usock, info, sizeof(info), 0);

	if(sent < 0){
		perror("send");
		return -1;
	}

	while(sent < len){
		sent = send(usock, buffer, sizeof(buffer), 0);	

		if(sent < 0){
			perror("send");
			return -1;
		}
		buffer = buffer + sent;
	}

	free(info);
	return  0;
}