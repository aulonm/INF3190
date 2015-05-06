#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <time.h>





int main(int argc, char *argv[]){

	if(argc != 4){
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
	const long filesize = argv[3];
	const char* port = argv[4];

	uint8_t portnr = strtoul(port, 0, 10);

	int usock = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	if(usock == -1){
		perror("socket");
		return -1;
	}

	char buffer[filesize];
	
	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));

	if(connect(usock, (struct sockaddr*)&bindaddr, sizeof(bindaddr))){
		perror("Problems with connect");
		return -3;
	}

	// SEND THE PORTINFO TO THE TP DAEMON
	char dummybuf[1];
	dummybuf[0] = portnr;
	ssize_t sent = send(usock, dummybuf, strlen(dummybuf), 0);
	if(sent < 0){
		perror("Send");
		return -1;
	}

	int ackCounter = 0;
	while(1){
		char buf[1500];
		ssize_t recvd = recv(usock, buf, sizeof(buf), 0);
		if(recvd > 0){
			printf("Received '%s' from tpdaemon\n", strlen(buf));
			strcat(buffer, buf);
			if(strlen(buffer) == filesize){
				FILE *fp;
				int ch;
				fp = fopen(filename, "w+");
				for(ch = 0; ch < strlen(buffer); ch++){
					fputc(buffer[ch], fp);
				}
				fclose(fp);
			}
		}
	}



	
	return 0;
}