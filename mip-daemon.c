#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>



static int get_if_hwaddr(int sock, const char* devname, uint8_t hwaddr[6]){
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));

	assert(strlen(devname) < sizeof(ifr.ifr_name));
	strcpy(ifr.ifr_name, devname);

	if(ioctl(sock, SIOCGIFHWADDR, %ifra) < 0){
		perror("ioctl");
		return -1;
	}

	memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, 6*sizeof(uint8_t));

	return 0;
}



int main(int argc, char* argv[]){
	int sock1, sock2;
	const char* sockname
;
	if(argc != 2){
		printf("USAGE: %s [socket]\n", argv[0]);
		printf("interface: The interface to send packets on\n");
		return -2;
	}

	sockname = argv[1];

	sock1 = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	sock2 = socket(AF_PACKET, SOCK_RAW, 0);

	if(sock1 == -1){
		perror("socket");
		return -3;
	}


	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));

	int retv = bind(sock1, (struct sockaddr*)&bindaddr, sizeof(bindaddr.sun_path));

	if(retv != 0){
		perror("bind");
		return -4;
	}

	if(listen(sock1, 5)){
		perror("listen");
		return -5;
	}

	fd_set rdfds;

	

	while(1){
		int cfd;
		FD_ZERO(&rdfds);
		FD_SET(sock1, &rdfds);
		FD_SET(sock2, &rdfds);


		int maxfd = sock2;

		retv = select(maxfd+1, &rdfds, NULL, NULL, NULL);

		if(retv <= 0){
			perror("select");
			return -6;
		}


		// IF AF_UNIX SOCKET
		if(FD_ISSET(sock1, &rdfds)){
			cfd = accept(sock1, NULL, NULL);
			printf("New connection! %d\n", cfd);
			char rbuf[100];
			ssize_t recvd = recv(cfd, rbuf, 99, 0);
			printf("Recieved %zd bytes from client %s\n", cfd, rbuf);
			send(cfd, "Pong!", 5, 0);
		}
		// IF RAW SOCKET
		if(FD_ISSET(sock2, NULL, NULL)){

		}


	}
	close(sock1);
	close(sock2);
	unlink(sockname);
}
