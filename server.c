#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char* argv[]){

	if(argc != 2){
		printf("USAGE: %s [socket name]\n", argv[0]);
		return -1;
	}

	const char* sockname = argv[1];
	int usock = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	if(usock == -1){
		perror("socket");
		return -2;
	}

	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));

	if(bind(usock, (struct sockaddr*)&bindaddr, sizeof(bindaddr))){
		perror("bind");
		return -3;
	}

	if(listen(usock, 5)){
		perror("listen");
		return -4;
	}

	while(1){
		int cfd = accept(usock, NULL, NULL);
		char buf[100];
		ssize_t recvd = read(cfd, buf, 99);

		if(recvd > 0){
			buf[recvd] = 0;
			printf("Received '%s' from client\n", buf);
			write(cfd, "Pong", 6);
		}
		close(cfd);
	}

	close(usock);
	unlink(sockname);
}