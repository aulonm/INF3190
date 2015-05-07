#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>

#include <sys/ioctl.h>
#include <bits/ioctls.h>

#define TOTAL_CONNECTS 5

struct tp_packet {
	unsigned int PL: 2;
	unsigned int port: 14;
	unsigned int seqnr: 16;
	char contents[0];
} __attribute__((packed));


struct connection {
	unsigned int port: 14;
	int cfd;
	char filebuf[0];
} __attribute__((packed));

struct info {
	unsigned int port: 14;
}__attribute__((packed));

int clientSocket, mipSocket;
const char *sock1, *sock2;
int packet_recvd = 0; /* highest packet successfully received */
int packet_sent = 0; /* highest packet sent */
struct connection connections[TOTAL_CONNECTS];
struct tp_packet window[10];
int windowCounter = 0;



int get_random_number(){
	srand(time(NULL));
	int i;
	int counter;
	int r;
	for(i = 0; i < TOTAL_CONNECTS; i++){
		if(connections[i].cfd != 0){
			counter++;
		}
	}

	r = rand() % counter;

	for(i = 0; i < TOTAL_CONNECTS; i++){

	}

	return 0;

}

int main(int argc, char *argv[]){


	if(argc != 3){
		printf("USAGE: %s [sock1] [sock2] [-d]\n", argv[0]);
		printf("Sock1: Socket to connecto to\n");
		printf("Sock2: Socket to connect from\n");
		return -1;
	}

	sock1 = argv[1];
	sock2 = argv[2];

	clientSocket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	//mipSocket = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	if(clientSocket == -1){
		perror("ClientSocket");
		return -1;
	}

	// if(mipSocket == -1){
	// 	perror("mipSocket");
	// 	return -1;
	// }

	// Bind the AF_UNIX socket
	struct sockaddr_un bindaddrClient;
	bindaddrClient.sun_family = AF_UNIX;
	strncpy(bindaddrClient.sun_path, sock1, sizeof(bindaddrClient.sun_path));
	int retv = bind(clientSocket, (struct sockaddr *)&bindaddrClient, sizeof(bindaddrClient.sun_path));

	/* If it doesnt return 0, it couldnt bind to the socket */
	if(retv != 0){
		perror("bind client");
		return -1;
	}

	
	// struct sockaddr_un bindaddr;
	// bindaddr.sun_family = AF_UNIX;
	// strncpy(bindaddr.sun_path, sock2, sizeof(bindaddr.sun_path));

	// if(connect(mipSocket, (struct sockaddr*)&bindaddr, sizeof(bindaddr))){
	// 	perror("Connect");
	// 	return -1;
	// }


	if(listen(clientSocket, 5)){
		perror("listen");
		return -1;
	}

	fd_set rdfds;
	int i;

	while(1){
		int maxfd = -1;
		FD_ZERO(&rdfds);
		FD_SET(clientSocket, &rdfds);
		//FD_SET(mipSocket, &rdfds);
		maxfd = clientSocket;

		if(clientSocket > maxfd) maxfd = clientSocket;
		//if(mipSocket > maxfd) maxfd = mipSocket;
		
		for(i = 0; i < TOTAL_CONNECTS; i++){
			if(connections[i].cfd != 0){
				FD_SET(connections[i].cfd, &rdfds);
				if(connections[i].cfd > maxfd) maxfd = connections[i].cfd;
			}
		}
		retv = select(maxfd+1, &rdfds, NULL, NULL, NULL);
		if(retv <= 0){
			perror("select");
			return -1;
		}

		if(FD_ISSET(mipSocket, &rdfds)){
			char buf[1500];
			ssize_t recvd = recv(mipSocket, buf, sizeof(buf), 0);
			if(recvd < 0){
				perror("read");
				return -1;
			}
			uint8_t mipaddr = buf[0];
			struct tp_packet* packet = (struct tp_packet*)buf;
			// Checks if it is an ack, 5 bytes. 1 byte for mip, 4 bytes for tp header
			if(recvd == 5){
				//Check if seqnr is the expected nr
				if(packet->seqnr == packet_sent){
					packet_sent++;
					//more packets
				}
				// Not the expected, so resend the packets
				else{

				}
			} else{
				for(i = 0; i < TOTAL_CONNECTS; i++){
					// If it is the right portnr
					if(connections[i].port == packet->port){
						if(packet->seqnr <= packet_recvd){
							// Is it the right packet
							if(packet->seqnr == packet_recvd){
								//Send up to server
								ssize_t sent = send(connections[i].cfd, packet->contents, sizeof(packet->contents), 0);
								if(sent < 0){
									perror("send");
									return -1;
								}
								packet_recvd++;
							}
							memset(buf, 0, sizeof(buf));
							buf[0] = mipaddr;
							//Send ack back
							struct tp_packet* ackPacket;
							ackPacket = malloc(sizeof(struct tp_packet));
							ackPacket->port = packet->port;
							ackPacket->PL = 0;
							ackPacket->seqnr = packet_recvd;
							strcat(buf, ackPacket);

							ssize_t sent = send(mipSocket, buf, sizeof(buf), 0);
							//Send the ack back to the other tp mip, dont forget to put the mip address too
							if(sent < 0){
								perror("send");
								return -1;
							}
						}
					}
				}
			}
		}

		if(FD_ISSET(clientSocket, &rdfds)){
			printf("hello clientSocket, is it me you are looking for\n");
			int cfd = accept(clientSocket, NULL, NULL);

			for(i = 0; i < TOTAL_CONNECTS; i++){
				if(connections[i].cfd == 0){
					connections[i].cfd = cfd;
					break;
				}
			}
			if(i == TOTAL_CONNECTS){
				close(cfd);
			}
			if(connections[i].cfd != 0){
				printf("Hello this is cfd\n");
				char buf[1492];
				ssize_t recvd = recv(connections[i].cfd, buf, sizeof(buf), 0);
				if(recvd < 0){
					perror("receive");
					return -1;
				}else if(recvd == 0){
					close(connections[i].cfd);
					connections[i].cfd = 0;
					connections[i].port = 0; 
					continue;
				}
				if(connections[i].port == 0){
					struct info* info;
					info = (struct info*)buf;
					printf("port: %u\n", info->port);
					printf("port on connection %u\n", connections[i].cfd);

					connections[i].port = info->port;
				}	
			}
		}

		/*for(i = 0; i < TOTAL_CONNECTS; i++){
			if(connections[i].cfd != 0 && FD_ISSET(connections[i].cfd, &rdfds)){
				printf("hello cfd, is it me you are looking for\n");
				
				// PUT THE STUFF IN A BUFFER
				char buf[1500]; // NOT FINAL YO
				memset(buf, 0, sizeof(buf));

				ssize_t recvd = recv(connections[i].cfd, buf, sizeof(buf), 0);

				if(recvd < 0){
					perror("read");
					return -1;
				} else if(recvd == 0){
					close(connections[i].cfd);
					connections[i].cfd = 0;
					connections[i].port = 0; 
					continue;
				}
				printf("cfd: %d\n",connections[i].cfd);

				// If this is the first time we are getting info from this connection, store info 
				if(connections[i].port == 0){
					struct info* info;
					info = (struct info*)buf;
					printf("port: %u\n", info->port);
					printf("port on connection %u\n", connections[i].cfd);

					connections[i].port = info->port;
				}
			}
		}*/
	}



	close(clientSocket);
//	close(mipSocket);
	return 0;
}