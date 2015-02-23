#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>




int main(int argc, char* argv[]){
	int sock1, sock2;
	const char* sockname
;
	if(argc != 2){
		printf("USAGE: %s [socket]\n", argv[0]);
		printf("interface: The interface to send packets on\n");
		return -1;
	}

	sockname = argv[1];

	sock1 = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	sock2 = socket(AF_PACKET, SOCK_SEQPACKET, 0);

	if(sock1 == -1){
		perror("socket");
		return -2;
	}


	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));

	int retv = bind(sock1, (struct sockaddr*)&bindaddr, sizeof(bindaddr.sun_path));

	if(retv != 0){
		perror("bind");
		return -3;
	}

	if(listen(sock1, 5)){
		perror("listen");
		return -4;
	}

	fd_set rdfds;

	

	while(1){
		int cfd;
		FD_ZERO(&rdfds);
		FD_SET(sock1, &rdfds);

		int maxfd = sock1;

		retv = select(maxfd+1, &rdfds, NULL, NULL, NULL);

		if(retv <= 0){
			perror("select");
			return -5;
		}

		if(FD_ISSET(sock1, &rdfds)){
			cfd = accept(sock1, NULL, NULL);
			printf("New connection! %d\n", cfd);
			char rbuf[100];
			ssize_t recvd = recv(cfd, rbuf, 100, 0);
			printf("Recieved %zd bytes from client %s\n", cfd, rbuf);
			send(cfd, "Pong!", 5, 0);
		}
		


	}
	close(sock1);
	unlink(sockname);




	/*while(1){
		FD_ZERO(&rdfds);
		FD_SET(sock1, &rdfds);
		int maxfd = sock2;

		int retv = select(maxfd+1, &rdfds, NULL, NULL, NULL);

		if(retv <= 0){
			perror("select");
			return -5;
		}

		if(FD_ISSET(sock1, &rdfds)){
			int cfd = accept(sock1, NULL, NULL);
			printf("New connection! %d\n", cfd);

			char rbuf[100];
			ssize_t recvcnt = recv(sock1, rbuf, 100, 0);
			if(recvcnt <= 0){
				close(sock1);
				continue;
			}
			printf("Received %zd bytes from client", recvcnt);


		} else if(FD_ISSET(sock2, &rdfds)){

		}

	}*/

}
