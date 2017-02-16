#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <time.h>

struct info {
	unsigned int port: 14;
}__attribute__((packed));



int main(int argc, char *argv[]){

	if(argc != 5){
		printf("USAGE: %s [socket] [filename] [filesize] [port]", argv[0]);
		printf("Socket: Socket you want to connect to\n");
		printf("Filename: Filename of the new file\n");
		printf("Filsize: The filesize of the new file\n");
		printf("Port: Port number\n");
		return -1;
	}

	const char* sockname = argv[1];
	const char* filename = argv[2];
	//filesize, what type of variable?
	const long filesize = strtoul(argv[3], 0, 10);
	const char* port = argv[4];

	unsigned int portnr = strtoul(port, 0, 10);

	int usock = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	if(usock == -1){
		perror("socket");
		return -1;
	}
	printf("hei\n");
	char buffer[filesize];
	printf("hei\n");
	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));

	if(connect(usock, (struct sockaddr*)&bindaddr, sizeof(bindaddr))){
		perror("Problems with connect");
		return -3;
	}

	// SEND THE PORTINFO TO THE TP DAEMON
	struct info* info;
    info = malloc(sizeof(struct info));
    info->port = portnr;

    printf("port %u\n", info->port);
	ssize_t sent = send(usock, info, sizeof(info), 0);
	if(sent < 0){
		perror("Send");
		return -1;
	}

	while(1){
		char buf[1492];
		ssize_t recvd = recv(usock, buf, sizeof(buf), 0);
		if(recvd > 0){
			// Set in the buffer we received, strcat it into the big buffer
			strcat(buffer, buf);
			// If the bufferlength is the same as filesize we got in commandline,
			// then write to the new file with new name
			if(strlen(buffer) == filesize){
				FILE *fp;
				int ch;
				fp = fopen(filename, "w+");
				for(ch = 0; ch < strlen(buffer); ch++){
					fputc(buffer[ch], fp);
				}
				printf("Written %d bytes\n", strlen(buffer));
				fclose(fp);
			}
		}
	}
	return 0;
}