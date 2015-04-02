#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>

#include <sys/ioctl.h>
#include <bits/ioctls.h>

#define ETH_P_MIP 65535

/**
 * Struct definition of a MIP Packet
 */
struct mip_packet {
	unsigned int TRA: 3;
	unsigned int TTL: 4;
	unsigned int payload: 9;
	uint8_t src_addr;
	uint8_t dst_addr;
	char    contents[0];
} __attribute__((packed));

/**
 * Struct definition of an ethernet frame
 */
struct ether_frame {
	uint8_t dst_addr[6];
	uint8_t src_addr[6];
	uint8_t eth_proto[2];
	uint8_t contents[0];
} __attribute__((packed));

struct neighbour{
	uint8_t neighbour_mip;
	uint8_t src_mip;
	uint8_t own_mac[6];
	int socket;
} __attribute__((packed));




int clientSocket, networkSocket, i;
int debug = 0;						// Debug mode
uint8_t iface_hwaddr[6];			// Mac address array
uint8_t arp_cache[256][6];			// ARP-cache to holde the MAC addrs
uint8_t arp_check[256];				// ARP-check to know if there are any addrs there
uint8_t routing_table[256][2];		// The routing table
int counter;						// Keep in check how long the neighbour-struct is

uint8_t addr;						// Address where to send messages
const char *sockname;				// socket name
const char *interface;				// The interface to send packets to
const char *stringFile; 			// The feil we are going to read


struct ether_frame* frame;
size_t msgsize;
struct mip_packet* mip;
size_t mipsize;

/**
*   Retrieves the hardware address of the given network device
*
*   @param sock     Socket to use for the IOCTL
*   @param devname  Name of the network device (for example eth0)
*   @param hwaddr   Buffer to write the hardware address to
*   @param Zero on succes, -1 otherwise
**/
static int get_if_hwaddr(int sock, const char *devname, uint8_t hwaddr[6]) {
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));

	assert(strlen(devname) < sizeof(ifr.ifr_name));
	strcpy(ifr.ifr_name, devname);

	if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
		perror("ioctl");
		return -1;
	}

	memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, 6 * sizeof(uint8_t));

	return 0;
}
/**
*	Prints the mac-address
*	@param mac The mac address to print
*/
static void printmac(uint8_t mac[6]) {
	int i;
	for (i = 0; i < 5; i++) {
		printf("%02x:", mac[i]);
	}
	printf("%02x\n", mac[5]);
}

int read_and_intialize(){
	return 0;
}

int update_routingtable(uint8_t recvd[256][2], uint8_t recvd_mip){
	
	for(i = 0; i < sizeof(routing_table); i++){
		if(recvd[i][0] > 0 && recvd[i][0]+1 < routing_table[i][0]){
			routing_table[i][0] = recvd[i][0]+1;
			routing_table[i][1] = recvd_mip;
		}
	}
	return 0;
}

int arp_request(char rbuf[1500]){
	mipsize = sizeof(struct mip_packet);
	mip = malloc(mipsize);
	assert(mip);

	printf("%u\n", rbuf[0]);
	mip->TRA = 1;
	mip->TTL = 15;
	mip->payload = 0;
	mip->dst_addr = rbuf[0];
	mip->src_addr = addr;

	msgsize = sizeof(struct ether_frame) + mipsize;
	frame = malloc(msgsize);

	/* Ethernet destination address */
	memcpy(frame->dst_addr, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);

	/* Fill in our source address */
	memcpy(frame->src_addr, iface_hwaddr, 6);

	/* Ethernet protocol field = 0xFFFF (MIP) */
	frame->eth_proto[0] = frame->eth_proto[1] = 0xFF;
		
	/* Fill in the message */
	memcpy(frame->contents, mip, mipsize);

	/* Send the packet */
	ssize_t retv = send(networkSocket, frame, msgsize, 0);

	free(mip);
	free(frame);

	if(retv < 0){
		perror("ARP Request send");
		return -14;
	}

	if(debug){
		printf("Arp request:\n");
		printf("\tFrom mip: %u, to mip: %u\n", addr, (uint8_t)rbuf[0]);
		printf("\tSource ethernet address: ");
		printmac(iface_hwaddr);
	}

	while(1){
		char buf[1500];
		memset(buf, 0, sizeof(buf));

		frame = (struct ether_frame*)buf;

		ssize_t retv = recv(networkSocket, buf, sizeof(buf), 0);

		if(retv < 0){
			perror("ARP Response receive");
			return -15;
		}

		mip = (struct mip_packet*)frame->contents;

		
		/* If TRA bits are 000 then it is an ARP Response */
		if((mip->TRA == 0) && (mip->dst_addr == addr)){
			if(debug){
				printf("Got ARP response:\n");
				printf("\tFrom mip: %u\n", mip->src_addr);
			}

			/* Cache the MAC address */
			memcpy(arp_cache[mip->src_addr], frame->src_addr, 6);
			arp_check[mip->src_addr] = 1;

			break;
		}
	}
	return 0;
}

int arp_response(struct mip_packet* recvd_mip){
	uint8_t MIP_dest = recvd_mip->src_addr;
	printf("Sending arp response\n");
	mipsize = sizeof(struct mip_packet);
	mip = malloc(mipsize);
	assert(mip);

	mip->TRA = 0;
	mip->TTL = 15;
	mip->payload = 0;
	mip->src_addr = addr;
	mip->dst_addr = MIP_dest;

	msgsize = sizeof(struct ether_frame) + sizeof(mip);
	frame = malloc(msgsize);
	assert(frame);

	memcpy(frame->dst_addr, arp_cache[MIP_dest], 6);
	memcpy(frame->src_addr, iface_hwaddr, 6);
	frame->eth_proto[0] = frame->eth_proto[1] = 0xFF;
	memcpy(frame->contents, mip, mipsize);
	ssize_t retv = send(networkSocket, frame, msgsize, 0);

	free(mip);
	free(frame);

	if(retv < 0){
		perror("ARP response send");
		return -17;
	}

	// Sent ARP Response back
	if(debug){
		printf("Responding to arp request from MIP %u", mip->dst_addr);
	}

	memcpy(arp_cache[MIP_dest], frame->src_addr, 6);
	arp_check[MIP_dest] = 1;
	
	return 0;
}

int clientHandler(char rbuf[1500], ssize_t recvd){
	/* If the arp cache table doesnt have the address, send an arp request */
	/* ARP REQUEST */
	if(arp_check[(unsigned int)rbuf[0]] != 1){
		/* Start making the Ether frame */
		arp_request(rbuf);
	}

	if(debug){
		printf("Arp Table:\n");
		int i;
		for(i = 0; i < 256; i++){
			if(arp_check[i]){
				printf("MIP: %u - Mac: ", i);
				printmac(arp_cache[i]);
			}
		}
	}

	/**
	* Now the arp cache table should have the addres
	* and we can continue to send our message to the other mip 
	* Transport happens from here
	* First we make our MIP packet:
	**/
	mipsize = sizeof(struct mip_packet) + recvd - 1;
	mip = malloc(mipsize);
	assert(mip);

	mip->TRA = 4;										// TRA = 4 = 100
	mip->TTL = 15;										// TTL = 15 = 1111
	mip->payload = recvd - 1;							// Payload
	mip->dst_addr = rbuf[0];							// Destination addr
	mip->src_addr = addr;								// Source addr
	memcpy(mip->contents, rbuf+1, recvd - 1); 			// Copy buf to contents

	msgsize = sizeof(struct ether_frame) + mipsize;
	frame = malloc(msgsize);
	assert(frame);

	memcpy(frame->dst_addr, arp_cache[(unsigned int)rbuf[0]], 6);
	memcpy(frame->src_addr, iface_hwaddr, 6);
	frame->eth_proto[0] = frame->eth_proto[1] = 0xFF;
	memcpy(frame->contents, mip, mipsize);

	ssize_t retv = send(networkSocket, frame, msgsize, 0);
	
	if(retv < 0){
		perror("raw send");
		return -11;
	}

	if(debug){
		printf("Sending to the other MIP:\n");
		printf("\tSource Mac: 		");
		printmac(frame->src_addr);
		printf("\tDestination Mac: 	");
		printmac(frame->dst_addr);
		printf("\tMIP source: 		%u\n", mip->src_addr);
		printf("\tMIP Destination: 	%u\n", mip->dst_addr);
	}

	free(mip);
	free(frame);

	return 0;
}

int networkHandler(int cfd){
	/* Receive the ethernet frame from an other MIP */
	char buf[1500];
	memset(buf, 0, sizeof(buf));
	frame = (struct ether_frame*)buf;
	ssize_t retv = recv(networkSocket, buf, sizeof(buf), 0);

	if(retv < 0){
		perror("Network receive");
		return -12;
	}
	mip = (struct mip_packet*)frame->contents;
	
	// Tell them what you got, mip and mac
	if(debug){
		printf("Receiving ether frame from other MIP:\n");
		printf("\tSource Mac: 		");
		printmac(frame->src_addr);
		printf("\tDestination Mac: 	");
		printmac(frame->dst_addr);
		printf("\tMIP source: 		%u\n", mip->src_addr);
		printf("\tMIP Destination: 	%u\n", mip->dst_addr);
	}
	
	/* Check if it is a transport and destination is this mip-daemon */
	if((mip->TRA == 4) && (mip->dst_addr == addr)){
		char* sbuf[1500];
		memset(sbuf, 0, sizeof(sbuf));
		sbuf[0] = mip->src_addr;
		strcat(sbuf, mip->contents);

		/* Send message to server */
		ssize_t sent = send(cfd, sbuf, strlen(sbuf), 0);

		if(sent < 0){
			perror("Send");
			return -16;
		}

		// Tell them what you sent to the client
		if(debug){
			printf("Sent to client: %s\n", sbuf);
		}
	}

	/* Check if it is an ARP Request, if it is then start making ARP Response and send */
	else if((mip->TRA == 1) && (mip->dst_addr == addr)){
		arp_response(mip);
	}

	else if((mip->TRA == 2) && (mip->dst_addr == addr)){

	}

	return 0;
}

int main(int argc, char *argv[]) {

	if(argc < 2 || argc > 3){
		printf("USAGE: %s [socket] [file] [-d]\n", argv[0]);
		printf("Socket: Socket to connect to\n");
		printf("File: File with all the information for mips/interfaces etc\n");
		printf("-d: Debug mode\n");
		return -2;
	}


	/* Set the mandatory arguments to its respective variable */

	sockname = argv[1];
	stringFile = argv[2];

	/* Checks if there are any optional command-line args */
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-d") == 0) {
			debug = 1;
		}
	}
	
	/* Reset the arrays to 0 */
	memset(&arp_cache, 0, sizeof(arp_cache));
	memset(&arp_check, 0, sizeof(arp_check));
	memset(&routing_table, 0, sizeof(routing_table));

	

	FILE *myFile;
	myFile = fopen(stringFile, "r");
	unsigned int src_mip;
	unsigned int neighbour_mip;
	char interface[20];

	if(myFile == NULL){
		perror("Reading file:");
		return -1;
	}

	// To find out how many of the struct neighbour we need
	while(fscanf(myFile, "%u %u %s", &src_mip, &neighbour_mip, interface) != EOF){
		counter++;
	}

	rewind(myFile);

	struct neighbour neigh[counter];

	for(i = 0; i < counter; i++){
		if(fscanf(myFile, "%u %u %s", &src_mip, &neighbour_mip, interface) != EOF){
			neigh[i].neighbour_mip = neighbour_mip;
			neigh[i].src_mip = src_mip;
			/**
			* AF_PACKET = raw socket interface
			* SOCK_RAW  = we cant the 12 header intact (SOCK_DGRAM removes header)
			* ETH_P_ALL = all ethernet protocols
			**/
			neigh[i].socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_MIP));
			
			/* If the networksSocket returns -1 there is a problem */
			if(neigh[i].socket == -1){
				perror("Raw Socket");
				return -1;
			}

			/* Gets the MAC address, and if the return value is not 0, exit */
			if(get_if_hwaddr(neigh[i].socket, interface, neigh[i].own_mac) != 0){
				return -1;
			}

			/* Bind the networksocket to the specified interface */
			struct sockaddr_ll device;
			memset(&device, 0, sizeof(device));
			device.sll_family = AF_PACKET;
			device.sll_ifindex = if_nametoindex(interface);
			if(bind(neigh[i].socket, (struct sockaddr *)&device, sizeof(device))){
				perror("Could not bind raw socket");
				close(neigh[i].socket);
				return -6;
			}

			// Intialize the routing table
			routing_table[neighbour_mip][0] = 1;
			routing_table[neighbour_mip][1] = neighbour_mip;

			/* Print the hardware address of the interface */
			printmac(neigh[i].own_mac);
		}
	}

	fclose(myFile);

	/**
	*   AF_UNIX         =
	*   SOCK_SEQPACKET  =
	*   0               =
	**/
	clientSocket = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	
	//networkSocket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_MIP));

	/* If the clientsocket returns -1 there is a problem */
	if(clientSocket == -1){
		perror("ClientSocket");
		return -3;
	}

	/* If the networksSocket returns -1 there is a problem */
	// if(networkSocket == -1){
	// 	perror("Raw socket");
	// 	return -4;
	// }

	/* Gets the MAC address, and if the return value is not 0, exit */
	// if(get_if_hwaddr(networkSocket, interface, iface_hwaddr) != 0){
	// 	return -5;
	// }

	/* Print the hardware address of the interface */
	//printmac(iface_hwaddr);

	// Bind the networksocket to the specified interface
	// struct sockaddr_ll device;
	// memset(&device, 0, sizeof(device));
	// device.sll_family = AF_PACKET;
	// device.sll_ifindex = if_nametoindex(interface);

	// if(bind(networkSocket, (struct sockaddr *)&device, sizeof(device))){
	// 	perror("Could not bind raw socket");
	// 	close(networkSocket);
	// 	return -6;
	// }

	// Bind the AF_UNIX socket
	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));
	int retv = bind(clientSocket, (struct sockaddr *)&bindaddr, sizeof(bindaddr.sun_path));

	/* If it doesnt return 0, it couldnt bind to the socket */
	if(retv != 0){
		perror("bind");
		return -7;
	}

	/* Listens to incoming connections from client programs */
	if(listen(clientSocket, 5)){
		perror("listen");
		return -8;
	}

	fd_set readfds;
	int cfd = -1;

	while(1){
		/* Clear the set ahead of time */
		FD_ZERO(&readfds);

		/* If the cfd is less then 0, add a clientSocket to the set */
		if(cfd < 0){
			FD_SET(clientSocket, &readfds);
		}
		/* Else add the cfd to the set */
		else{
			FD_SET(cfd, &readfds);
		}

		/* Add all the networkSockets to the set */
		FD_SET(networkSocket, &readfds);
		for(i = 0; i < counter; i++){
			FD_SET(neigh[i].socket, &readfds);
		}
		int maxfd;

		/* Finds out the maxvalue of the file descriptors */
		if(cfd > maxfd) maxfd = cfd;
		if(clientSocket > maxfd) maxfd = clientSocket;
		if(networkSocket > maxfd) maxfd = networkSocket;
		for(i = 0; i < counter; i ++){
			if(neigh[i].socket > maxfd) maxfd = neigh[i].socket;
		}


		retv = select(maxfd + 1, &readfds, NULL, NULL, NULL);
		if(retv <= 0){
			perror("select");
			return -9;
		}

		/* If you get a cfd then this part happens, the client is sending messages */
		if(FD_ISSET(cfd, &readfds)){
			char rbuf[1500];
			ssize_t recvd = recv(cfd, rbuf, sizeof(rbuf), 0);
			if(recvd == 0){
				cfd = -1;
				continue;
			} else if(recvd < 0){
				perror("read");
				return -10;
			} 
			clientHandler(rbuf, recvd);
		}

		/* If we dont have any connection on that socket, then accept one */
		if(FD_ISSET(clientSocket, &readfds)){
			cfd = accept(clientSocket, NULL, NULL);
		}

		/* If you get a networkSocket then this will happen */
		if(FD_ISSET(networkSocket, &readfds)){
			networkHandler(cfd);
		}
	}
	close(clientSocket);
	close(networkSocket);
	unlink(sockname);
	return 0;
}
