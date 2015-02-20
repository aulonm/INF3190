#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>




int main(int argc, char* argv[]){
	int sock1, sock2;
	const char* interface;


	if(argc != 2){
		printf("USAGE: %s [interface]\n", argv[0]);
		printf("interface: The interface to send packets on\n");
		return -1;
	}

	interface = argv[1];

	sock1 = socket(AF_UNIX, SOCK_STREAM, 0);
	sock2 = socket(AF_PACKET, SOCK_SEQPACKET, 0);


	struct sockaddr_in bindaddr;
	memset(&bindaddr, 0, sizeof(bindaddr));
	bindaddr.sin_family = AF_UNIX;
	bindaddr.sin_addr.s_addr = &interface;

	int retv = bind(sock1, (struct sockaddr*)&bindaddr, sizeof(bindaddr));

	if(retv != 0){
		perror("bind");
		return -2;
	}

	if(listen(sock1, 5)){
		perror("listen");
		return -3;
	}

	fd_set rdfds;

	while(1){
		FD_ZERO(&rdfds);
		FD_SET(sock1, &rdfds);
		int maxfd = sock2;

		retv = select(maxfd+1, &rdfds, NULL, NULL, NULL);

		if(retv <= 0){
			perror("select");
			return -4;
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

	}

}
