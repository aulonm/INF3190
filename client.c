#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


int main(int argc, char* argv[]){


	// To check if there are enough arguments
	if(argc != 2){
		printf("USAGE: %s [socket name]\n", argv[0]);
		return -1;
	}

	const char* sockname = argv[1];
	int usock = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	if(usock == -1){
		perror("Wrong with socket");
		return -2;
	}

	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));

	if(connect(usock, (struct sockaddr*)&bindaddr, sizeof(bindaddr))){
		perror("Problems with connect");
		return -3;
	}

	write(usock, "Client", 6);
	char buf[100];
	ssize_t recvd = read(usock, buf, 99);

	if(recvd < 0){
		perror("Problems with read");
		return -4;
	}

	buf[recvd] = 0;
	printf("Received message: %.*s\n", recvd, buf);

	close(usock);
	return 0;

}