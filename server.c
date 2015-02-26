#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char* argv[]){

	char* pong = "Pong!";
	char* sbuf[100];
	strcpy(sbuf, pong);

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

	if(connect(usock, (struct sockaddr*)&bindaddr, sizeof(bindaddr))){
		perror("Problems with connect");
		return -3;
	}

	// if(bind(usock, (struct sockaddr*)&bindaddr, sizeof(bindaddr))){
	// 	perror("bind");
	// 	return -3;
	// }

	// if(listen(usock, 5)){
	// 	perror("listen");
	// 	return -4;
	// }

	while(1){
		char buf[100];
		ssize_t recvd = recv(usock, buf, 99, 0);

		if(recvd > 0){
			buf[recvd] = 0;
			printf("Received '%s' from client\n", buf);
			sbuf[0] = buf[0];
			strcat(sbuf, pong);
			send(usock, sbuf, sizeof(buf), 0);
			memset(&sbuf, 0, sizeof(sbuf));
		}
	}

	close(usock);
	unlink(sockname);
}