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
#define MAX_WINDOW 10

/**
*	TP_packet struct, with mip as an extra, easier to store mip addr
**/
struct tp_packet {
	uint8_t mip;
	unsigned int PL: 2;
	unsigned int port: 14;
	unsigned int seqnr: 16;
	char contents[0];
} __attribute__((packed));

/**
*	Struct to store all connections with their appropriate portnr
**/
struct connection {
	unsigned int port: 14;
	int cfd;
} __attribute__((packed));

/**
*	Struct used to get port fra client/server
**/
struct info {
	unsigned int port: 14;
}__attribute__((packed));

const char *sock1, *sock2;
char window[10][1500]; //tp packet, mip address and data
int clientSocket, mipSocket;
int packet_recvd = 0; /* highest packet successfully received */
int packet_sent = 0; /* highest packet sent */
int windowCounter = 0;
struct connection connections[TOTAL_CONNECTS];
struct tp_packet window_packets[10];

/**
*	The main method
*
*	@param	argc	nr of arguments
*	@param	argv[]	list of arguments
*	@param	Zero on success, -1 otherwise, or ERROR_SEND/RECV, ERROR_SELECT
**/
int main(int argc, char *argv[]){
	if(argc != 3){
		printf("USAGE: %s [sock1] [sock2] [-d]\n", argv[0]);
		printf("Sock1: Socket to connecto to\n");
		printf("Sock2: Socket to connect from\n");
		return -1;
	}

	sock1 = argv[1];
	sock2 = argv[2];

	/**
	*   AF_UNIX         =	Local communication
	*   SOCK_SEQPACKET  =	Reliable two-way connection-based data transmission
	*   0               =	Protocol we are using, in this case none
	**/
	clientSocket = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	/**
	*   AF_UNIX         =	Local communication
	*   SOCK_SEQPACKET  =	Reliable two-way connection-based data transmission
	*   0               =	Protocol we are using, in this case none
	**/
	mipSocket = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	/* If the clientsocket returns -1 there is a problem */
	if(clientSocket == -1){
		perror("ClientSocket");
		return -1;
	}

	/* If the MipSocket returns -1 there is a problem */
	if(mipSocket == -1){
		perror("mipSocket");
		return -1;
	}

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

	/* Connect the mipsocket */
	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sock2, sizeof(bindaddr.sun_path));
	if(connect(mipSocket, (struct sockaddr*)&bindaddr, sizeof(bindaddr))){
		perror("Connect");
		return -1;
	}

	/* Listens to incoming connections from client programs */
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
		FD_SET(mipSocket, &rdfds);
		maxfd = clientSocket;

		//Finds maxfd
		if(clientSocket > maxfd) maxfd = clientSocket;
		if(mipSocket > maxfd) maxfd = mipSocket;
		// Finds maxfd, only if there is room on the sliding window
		if(windowCounter < MAX_WINDOW){
			for(i = 0; i < TOTAL_CONNECTS; i++){
				if(connections[i].cfd != 0){
					FD_SET(connections[i].cfd, &rdfds);
					if(connections[i].cfd > maxfd) maxfd = connections[i].cfd;
				}
			}
		}
		
		retv = select(maxfd+1, &rdfds, NULL, NULL, NULL);
		if(retv <= 0){
			perror("select");
			return -1;
		}

		/* Checks the connections to see if there are any incoming packets */
		for(i = 0; i < TOTAL_CONNECTS; i++){
			if(connections[i].cfd != 0 && FD_ISSET(connections[i].cfd, &rdfds)){
				char rbuf[1500];
				// Receive from client
				ssize_t recvd = recv(connections[i].cfd, rbuf, 1493, 0);

				if(recvd == 0){
					close(connections[i].cfd);
					connections[i].cfd = 0;
					connections[i].port = 0;
					break;
				}

				printf("Got some data for you \n");
				printf("Received bytes %d\n", recvd);
				int j;
				for(j = 0; j < recvd; j++){
					printf("%d %c\n",j, rbuf[j]);
				}

				// Make a TP_Packet with the necessary information
				size_t packetsize = sizeof(struct tp_packet*) + recvd-1;
				struct tp_packet* packet = malloc(packetsize);
				assert(packet);
				packet->mip = (unsigned int)rbuf[0];
				packet->port = connections[i].port;
				packet->seqnr = packet_sent + windowCounter;
				packet->PL = ((recvd-1) % 4) == 0 ? 0 : 4 - ((recvd-1) % 4);
				memcpy(packet->contents, rbuf+1, 1492);


				// Debug information
				printf("Struct: %d\n", sizeof(struct tp_packet*));
				printf("size: %d\n", packetsize);
				printf("Mip: %u\n", packet->mip);
				printf("port: %u\n", packet->port);
				printf("seq: %u\n", packet->seqnr);
				printf("PL: %u\n", packet->PL); 
				printf("contents: %s\n", packet->contents);

				// Copy the packet into a window_packets buffer, to keep hold of the 10 packets that
				// we are waiting on acks
				memcpy(window[(packet_sent + windowCounter) % MAX_WINDOW], packet, packetsize);
				printf("buffer: %s\n", window[(packet_sent + windowCounter) % MAX_WINDOW]);

				//send the packet on mipsocket
				ssize_t sent = send(mipSocket, window[(packet_sent + windowCounter) % MAX_WINDOW], packetsize+packet->PL, 0);
				if(sent < 0){
					perror("send");
					return -1;
				}
				printf("Sentsize: %d\n", sent);
				//Window counter ++
				windowCounter++;
			}
		}


		// If we get anything from the mip, a packet?
		if(FD_ISSET(mipSocket, &rdfds)){
			char buf[1500];
			ssize_t recvd = recv(mipSocket, buf, sizeof(buf), 0);
			
			if(recvd < 0){
				perror("read");
				return -1;
			}

			// Debug information
			printf("nr of bytes: %d\n", recvd);
			printf("buffer: %s\n", buf);

			// Cast the buffer to a TP_struct
			struct tp_packet* packet = (struct tp_packet*)buf+1;

			// Debug information
			printf("Receivesize: %d\n", recvd);
			printf("Port: %u\n", packet->port);
			printf("Mip: %d\n", packet->mip);
			printf("SeqNr: %d\n", packet->seqnr);
			printf("contents: %s\n", packet->contents);

			// Checks if it is an ack, 5 bytes. 1 byte for mip, 4 bytes for tp header
			if(recvd == 5){
				printf("Got an ack \n");
				//Check if seqnr is the expected nr or higher
				if(packet->seqnr >= packet_sent){
					// Set the packet_sent should become the seqnr from the packet
					windowCounter -= (packet->seqnr - packet_sent) + 1;
					packet_sent += packet->seqnr;
				}
			} else{
				// Not an ack, so its a packet to send, check first if valid packet
				for(i = 0; i < TOTAL_CONNECTS; i++){
					// If it is the right portnr
					if(connections[i].port == packet->port){
						// If the packet->seqnr is less or equal to the packet we expected
						if(packet->seqnr <= packet_recvd){
							// Check if it is the correct packet seqnr with the one expacted
							if(packet->seqnr == packet_recvd){
								//Send up to server
								ssize_t sent = send(connections[i].cfd, packet->contents, recvd - (5+packet->PL), 0);
								if(sent < 0){
									perror("send");
									return -1;
								}
								printf("Sent %d bytes to server\n", sent);
								packet_recvd++;
							}
							// Change the PL value only, since the rest doesnt matter
							// This is where the ack is sent
							packet->PL = 0;

							ssize_t sent = send(mipSocket, buf, sizeof(struct tp_packet*)+1, 0);
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

		// IF first time clientSocket
		if(FD_ISSET(clientSocket, &rdfds)){			

			// Find the first open cfd
			for(i = 0; i < TOTAL_CONNECTS; i++){
				if(connections[i].cfd == 0) break;
			}
			// Check if it is under the total connects we can have
			if(i < TOTAL_CONNECTS){
				//Accept it and put it in the struct connections
				connections[i].cfd = accept(clientSocket, NULL, NULL);
				char buf[1492];

				//Receive the port information from client/server
				ssize_t recvd = recv(connections[i].cfd, buf, sizeof(buf), 0);

				// Store it
				struct info* info;
				info = (struct info*)buf;
				connections[i].port = info->port;
			}
		}
	}
	close(clientSocket);
	close(mipSocket);
	return 0;
}