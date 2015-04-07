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

/**
 *	Struct definition for datalink/datalink information
 */
struct datalink{
	uint8_t dstlink_mip;
	uint8_t src_mip;
	uint8_t own_mac[6];
	int socket;
} __attribute__((packed));

int clientSocket, networkSocket, i, j;
int debug = 0;						// Debug mode
uint8_t iface_hwaddr[6];			// Mac address array
uint8_t arp_cache[256][6];			// ARP-cache to holde the MAC addrs
uint8_t arp_check[256];				// ARP-check to know if there are any addrs there
uint8_t routing_table[256][2];		// The routing table
int counter;						// Keep in check how long the datalink-struct is
struct timeval tv;
uint8_t addr;						// Address where to send messages
const char *sockname;				// socket name
const char *interface;				// The interface to send packets to
const char *stringFile; 			// The feil we are going to read


struct ether_frame* frame;
size_t msgsize;
struct mip_packet* mip;
size_t mipsize;

/**
*	Prints out the elements of the given array
*
*	@param table 	The table we want to print out its information
**/
static void print_table(uint8_t table[256][2]){
	for(i = 0; i < 256; i++){
		if(table[i][0] != 0){
			printf("Routing table:\t Dest: %d \t Via: %u \t Cost: %u\n", i, table[i][1], table[i][0]);
		}
	}
}

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

/**
*	Set up the clientSocket, bind it and set up listening to incoming connections
*	
*	@param Zero on success,	-1 otherwise
**/
int bind_clientsocket(){
	/**
	*   AF_UNIX         =	Local communication
	*   SOCK_SEQPACKET  =	Reliable two-way connection-based data transmission
	*   0               =	Protocol we are using, in this case none
	**/
	clientSocket = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	/* If the clientsocket returns -1 there is a problem */
	if(clientSocket == -1){
		perror("ClientSocket");
		return -1;
	}

	// Bind the AF_UNIX socket
	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));
	int retv = bind(clientSocket, (struct sockaddr *)&bindaddr, sizeof(bindaddr.sun_path));

	/* If it doesnt return 0, it couldnt bind to the socket */
	if(retv != 0){
		perror("bind");
		return -1;
	}

	/* Listens to incoming connections from client programs */
	if(listen(clientSocket, 5)){
		perror("listen");
		return -1;
	}

	return 0;
}

/**
*	Set up the networksocket on that specific datalink/datalink, bind it
*	and get the mac address from the interface
*
*	@param	dl[]		array of all the datalink/datalink structs
*	@param	pos 		in which position are we going to work in the array
*	@param	interface 	the interface we are using on this datalink
*	@param	Zero on success, -1 otherwise
**/
int bind_networksocket(struct datalink dl[], int pos, char interface[20]){
	/**
	* AF_PACKET = raw socket interface
	* SOCK_RAW  = we cant the 12 header intact (SOCK_DGRAM removes header)
	* ETH_P_ALL = all ethernet protocols
	**/
	dl[pos].socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_MIP));

	/* If the networksSocket returns -1 there is a problem */
	if(dl[pos].socket == -1){
		perror("Raw Socket");
		return -1;
	}

	/* Gets the MAC address, and if the return value is not 0, exit */
	if(get_if_hwaddr(dl[pos].socket, interface, dl[pos].own_mac) != 0){
		return -1;
	}

	/* Bind the networksocket to the specified interface */
	struct sockaddr_ll device;
	memset(&device, 0, sizeof(device));
	device.sll_family = AF_PACKET;
	device.sll_ifindex = if_nametoindex(interface);
	if(bind(dl[pos].socket, (struct sockaddr *)&device, sizeof(device))){
		perror("Could not bind raw socket");
		close(dl[pos].socket);
		return -6;
	}

	return 0;
}

/**
*	Updates mips own routing table with the one given from the other mip
*
*	@param	recvd 		The table we received
*	@param	recvd_mip 	The mip we received the table from
**/
void update_routingtable(uint8_t recvd[256], uint8_t recvd_mip){
	for(i = 0; i < 256; i++){
		if(recvd[i] != 0){
			printf("Routing table:\t Dest: %d \t Cost: %u\n", i,recvd[i]);
		}			
	}

	for(i = 0; i < 256; i++){
		if(recvd[i] > 0 && (routing_table[i][0] == 0)){
			routing_table[i][0] = recvd[i]+1;
			routing_table[i][1] = recvd_mip;
		}
		if(recvd[i] > 0 && recvd[i]+1 < routing_table[i][0]){
			routing_table[i][0] = recvd[i]+1;
			routing_table[i][1] = recvd_mip;
		}
	}

	printf("After updating the routing table: \n");
	print_table(routing_table);
}

/* Makes an arp response frame and sends it back */
/**
*	Make and arp response frame and sends it back on the network
*
*	@param	recvd_frame	the frame we received, so that we can update our arp_cache
*	@param	dl 		the correct datalink we are on, so that we get the right information
*	@param 	Zero on success, -1 otherwise
**/
int arp_response(struct ether_frame* recvd_frame, struct datalink dl){
	mip = (struct mip_packet*)recvd_frame->contents;
	uint8_t MIP_dest = mip->src_addr;

	mipsize = sizeof(struct mip_packet);
	mip = malloc(mipsize);
	assert(mip);

	mip->TRA = 0;
	mip->TTL = 15;
	mip->payload = 0;
	mip->src_addr = dl.src_mip;
	mip->dst_addr = MIP_dest;

	msgsize = sizeof(struct ether_frame) + sizeof(mip);
	frame = malloc(msgsize);
	assert(frame);

	memcpy(frame->dst_addr, arp_cache[MIP_dest], 6);
	memcpy(frame->src_addr, dl.own_mac, 6);
	frame->eth_proto[0] = frame->eth_proto[1] = 0xFF;
	memcpy(frame->contents, mip, mipsize);
	ssize_t retv = send(dl.socket, frame, msgsize, 0);

	if(retv < 0){
		perror("ARP response send");
		return -1;
	}

	if(debug){
		printf("Responding to arp request from MIP %u", mip->dst_addr);
	}

	memcpy(arp_cache[MIP_dest], recvd_frame->src_addr, 6);
	arp_check[MIP_dest] = 1;	

	free(mip);
	free(frame);

	return 0;
}

/**
*	Makes and arp request frame and sends it out on the network
*
*	@param	dl 	The datalink we are on at the moment
*	@param 	Zero on success, -1 otherwise
**/
int arp_request(struct datalink dl){
	printf("Making arp request\n");
	mipsize = sizeof(struct mip_packet);
	mip = malloc(mipsize);
	assert(mip);

	mip->TRA = 1;
	mip->TTL = 15;
	mip->payload = 0; 
	mip->dst_addr = dl.dstlink_mip;
	mip->src_addr = dl.src_mip;

	msgsize = sizeof(struct ether_frame) + mipsize;
	frame = malloc(msgsize);
	assert(frame);

	/* Ethernet destination address */
	memcpy(frame->dst_addr, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);

	/* Fill in our source address */
	memcpy(frame->src_addr, dl.own_mac, 6);

	/* Ethernet protocol field = 0xFFFF (MIP) */
	frame->eth_proto[0] = frame->eth_proto[1] = 0xFF;
		
	/* Fill in the message */
	memcpy(frame->contents, mip, mipsize);

	/* Send the packet */
	ssize_t retv = send(dl.socket, frame, msgsize, 0);

	free(mip);
	free(frame);

	if(retv < 0){
		perror("ARP Request send");
		return -1;
	}

	if(debug){
		printf("Arp request:\n");
		printf("\tFrom mip: %u, to mip: %u\n", dl.src_mip, dl.dstlink_mip);
		printf("\tSource ethernet address: ");
		printmac(dl.own_mac);
	}

	while(1){
		printf("Waiting for arp response\n");
		char buf[1500];
		memset(buf, 0, sizeof(buf));

		frame = (struct ether_frame*)buf;

		ssize_t retv = recv(dl.socket, buf, sizeof(buf), 0);

		if(retv < 0){
			perror("ARP Response receive");
			return -1;
		}

		mip = (struct mip_packet*)frame->contents;

		/* If TRA bits are 000 then it is an ARP Response */	
		if((mip->TRA == 0) && (mip->dst_addr == dl.src_mip)){
			if(debug){
				printf("Got ARP response:\n");
				printf("\tFrom mip: %u\n", mip->src_addr);
			}

			/* Cache the MAC address */
			memcpy(arp_cache[mip->src_addr], frame->src_addr, 6);
			arp_check[mip->src_addr] = 1;

			break;
		}
		/* If TRA bits are 001 then it is an ARP Request and just send a respons back */
		else if((mip->TRA == 1) && (mip->dst_addr == dl.src_mip)){
			arp_response(frame, dl);
			break;
		}
	}
	return 0;
}


/**
*	A method to forward the frames that we have received and are not to us
*
*	@param	dl[]	The datalinks this mip has
*	@param	fframe 	The frame we have received
*	@param	Zero on success, -1 otherwise
**/
int forward_frame(struct datalink dl[], struct ether_frame* fframe){
	struct mip_packet* fmip = (struct mip_packet*)fframe->contents;
	uint8_t via;
	if(fmip->TTL == 0){
		printf("TTL IS UNDER 0");
		return 1;
	}
	for(i = 0; i < 256; i++){
		if(i == fmip->dst_addr && routing_table[i][0] != 0){
			via = routing_table[i][1];
		}
	}
	/* TTL-1 here */
	fmip->TTL = fmip->TTL-1;
	
	/* change the destination frame to the next datalink hop */
	memcpy(fframe->dst_addr, arp_cache[via], 6);
	ssize_t fsize = sizeof(struct ether_frame) + sizeof(struct mip_packet) + fmip->payload;
	
	/* Find the right source mac address who has the right datalink neighbour */
	for(i = 0; i < counter; i++){
		if(dl[i].dstlink_mip == via){
			memcpy(fframe->src_addr, dl[i].own_mac, 6);
			ssize_t retv = send(dl[i].socket, fframe, fsize, 0);

			if(retv < 0){
				perror("Forwarding");
				return -1;
			}
		}
	}
	return 0;
}

/**
*	Handles the information coming from the clientside
*
*	@param	rbuf	The buffer we received from the client
*	@param	recvd 	The size of the message
*	@param	dl[] Alle the datalinks we have stored to run through
*	@param	Zero on success, -1 otherwise
**/
int clientHandler(char rbuf[1500], ssize_t recvd, struct datalink dl[]){
	int pos, via;

	/* check if we have information about the destination in our routing table */
	if(routing_table[(uint8_t)rbuf[0]][0] != 0)
		via = routing_table[(uint8_t)rbuf[0]][1];

	/* Find the right position in the datalink array to use the right datalink in sending the frame */
	for(i = 0; i < counter; i++){
		if(dl[i].dstlink_mip == via)
			pos = i;
	}

	/* If the arp cache table doesnt have the address, send an arp request */
	/* ARP REQUEST */
	if(dl[pos].dstlink_mip == (uint8_t)rbuf[0])
		arp_request(dl[pos]);

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
	mip->TTL = routing_table[(uint8_t)rbuf[0]][0];										// TTL = 15 = 1111
	mip->payload = recvd - 1;							// Payload
	mip->dst_addr = rbuf[0];							// Destination addr
	mip->src_addr = dl[pos].src_mip;								// Source addr
	memcpy(mip->contents, rbuf+1, recvd - 1); 			// Copy buf to contents

	msgsize = sizeof(struct ether_frame) + mipsize;
	frame = malloc(msgsize);
	assert(frame);

	memcpy(frame->dst_addr, arp_cache[dl[pos].dstlink_mip], 6);
	memcpy(frame->src_addr, dl[pos].own_mac, 6);
	frame->eth_proto[0] = frame->eth_proto[1] = 0xFF;
	memcpy(frame->contents, mip, mipsize);

	ssize_t retv = send(dl[pos].socket, frame, msgsize, 0);
	
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

/**
*	Handles the frames coming in form the network
*
*	@param	cfd		the client socket
*	@param	dl[]	all the datalinks information
*	@param	pos 	the position we are at, at the moment, in the array of datalink structs
*	@param	Zero on success, -1 otherwise, 1 if TTL == -1
**/
int networkHandler(int cfd, struct datalink dl[], int pos){
	/* Receive the ethernet frame from an other MIP */
	char buf[1500];
	memset(buf, 0, sizeof(buf));
	frame = (struct ether_frame*)buf;
	ssize_t retv = recv(dl[pos].socket, buf, sizeof(buf), 0);

	if(retv < 0){
		perror("Network receive");
		return -12;
	}
	mip = (struct mip_packet*)frame->contents;
	
	// Tell them what you got, mip and mac
	if(debug){
		printf("Receiving ether frame from other MIP:\n");
		printf("\tThis MIP: %u\n", dl[pos].src_mip);
		printf("\tTRA: %u\n", mip->TRA);
		printf("\tSource Mac: 		");
		printmac(frame->src_addr);
		printf("\tDestination Mac: 	");
		printmac(frame->dst_addr);
		printf("\tMIP source: 		%u\n", mip->src_addr);
		printf("\tMIP Destination: 	%u\n", mip->dst_addr);
	}
	
	/* Check if it is a transport and destination is this mip-daemon */
	if((mip->TRA == 4) && (mip->dst_addr == dl[pos].src_mip)){
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
	else if((mip->TRA == 1) && (mip->dst_addr == dl[pos].src_mip)){
		arp_response(frame, dl[pos]);
	}

	/* Check if it is an routing packet */
	else if((mip->TRA == 2) && (mip->dst_addr == dl[pos].src_mip)){
		update_routingtable((uint8_t *)mip->contents, mip->src_addr);
	}

	/* Transport but this mip is not its destination, forward it */
	else if((mip->TRA == 4) && (mip->dst_addr != dl[pos].src_mip)){
		forward_frame(dl, frame);
	}
	return 0;
}

/**
*	The main method
*
*	@param	argc	nr of arguments
*	@param	argv[]	list of arguments
*	@param	Zero on success, -1 otherwise
**/
int main(int argc, char *argv[]) {

	if(argc < 2 || argc > 3){
		printf("USAGE: %s [socket] [file] [debug]\n", argv[0]);
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
		if (strcmp(argv[i], "debug") == 0) {
			debug = 1;
		}
	}

	debug = 1;

	/* Reset the arrays to 0 */
	memset(&arp_cache, 0, sizeof(arp_cache));
	memset(&arp_check, 0, sizeof(arp_check));
	memset(&routing_table, 0, sizeof(routing_table));

	/* Setting up to read from file */
	FILE *myFile;
	myFile = fopen(stringFile, "r");

	/* tmpvariables to use for reading from file */
	unsigned int src_mip;
	unsigned int dstlink_mip;
	char interface[20];

	/* Check if file is empty */
	if(myFile == NULL){
		perror("Reading file:");
		return -1;
	}

	/* To find out how many of the struct datalink we need */
	while(fscanf(myFile, "%u %u %s", &src_mip, &dstlink_mip, interface) != EOF){
		counter++;
	}

	/* Rewind the file so we can use it for real reading now */
	rewind(myFile);

	/* Set up an array of structs for dlbours/links */
	struct datalink dl[counter];

	/* Read the file, bind the sockets, and set the right variables in the struct */
	for(i = 0; i < counter; i++){
		if(fscanf(myFile, "%u %u %s", &src_mip, &dstlink_mip, interface) != EOF){
			dl[i].dstlink_mip = dstlink_mip;
			dl[i].src_mip = src_mip;
			
			bind_networksocket(dl, i, interface);

			// Intialize the routing table
			routing_table[dstlink_mip][0] = 1;
			routing_table[dstlink_mip][1] = dstlink_mip;

			/* Print the hardware address of the interface */
			printf("Interface nr %d: ", i);
			printmac(dl[i].own_mac);

		}
	}
	//print_table(routing_table);	

	fclose(myFile);

	/* Bind the clientsocket */
	bind_clientsocket();

	fd_set readfds;
	int cfd = -1;

	/* Set the timeout for the select */
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	while(1){
		int maxfd = -1;
		
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
		for(i = 0; i < counter; i++){
			FD_SET(dl[i].socket, &readfds);
		}
		
		/* Finds out the maxvalue of the file descriptors */
		if(cfd > maxfd)	maxfd = cfd;
		if(clientSocket > maxfd) maxfd = clientSocket;
		for(i = 0; i < counter; i ++){
			if(dl[i].socket > maxfd) maxfd = dl[i].socket;
		}

		int retv = select(maxfd + 1, &readfds, NULL, NULL, &tv);
		
		if(retv == -1){
			perror("select");
			return -9;
		} else if(retv == 0){
			tv.tv_sec = 1;
			/* Took over 1 sec to get anything from select, sending out routing tables! */
			for(i = 0; i < counter; i++){
				/* Checking if we have our datalinks MAC addresses, if not arp */
				if(arp_check[dl[i].dstlink_mip] != 1)
					arp_request(dl[i]);

				uint8_t tmp_routing[256];
				memset(tmp_routing, 0, sizeof(tmp_routing));

				/* Make 1D array of all the costs to the different destinations, dont need via-part */
				for(j = 0; j < 256; j++){
					if(routing_table[j][1] == dl[i].dstlink_mip){
						tmp_routing[j] = 0;
					} else{
						tmp_routing[j] = routing_table[j][0]; 
					}
				}

				// for(j = 0; j < 256; j++){
				// 	if(tmp_routing[j] != 0){
				// 		printf("Routing table:\t Dest: %d \t Cost: %u\n", j, tmp_routing[j]);			
				// 	}
				// }

				mipsize = sizeof(struct mip_packet) + sizeof(tmp_routing);
				mip = malloc(mipsize);
				assert(mip);

				mip->TRA = 2;
				mip->TTL = 15;
				mip->payload = sizeof(tmp_routing);
				mip->dst_addr = dl[i].dstlink_mip;
				mip->src_addr = dl[i].src_mip;
				memcpy(mip->contents, tmp_routing, sizeof(tmp_routing));

				msgsize = sizeof(struct ether_frame) + mipsize;
				frame = malloc(msgsize);
				assert(msgsize);
				memcpy(frame->dst_addr, arp_cache[dl[i].dstlink_mip], 6);
				memcpy(frame->src_addr, dl[i].own_mac, 6);
				frame->eth_proto[0] = frame->eth_proto[1] = 0xFF;
				memcpy(frame->contents, mip, mipsize);

				ssize_t retv = send(dl[i].socket, frame, msgsize, 0);

				if(retv < 0){
					perror("Send routing table");
					return -1;
				}

				free(frame);
				free(mip);
			}
		}

		/* If you get a cfd then this part happens, the client is sending messages */
		if(FD_ISSET(cfd, &readfds)){
			printf("Hello this is cfd");
			char rbuf[1500];
			ssize_t recvd = recv(cfd, rbuf, sizeof(rbuf), 0);
			if(recvd == 0){
				cfd = -1;
				continue;
				perror("read");
				return -10;
			} 
			clientHandler(rbuf, recvd, dl);
		}

		/* If we dont have any connection on that socket, then accept one */
		if(FD_ISSET(clientSocket, &readfds))
			cfd = accept(clientSocket, NULL, NULL);

		/* If you get a networkSocket then this will happen */
		for(i = 0; i < counter; i++){
			if(FD_ISSET(dl[i].socket, &readfds))	
				networkHandler(cfd, dl, i);
		}
	}
	close(clientSocket);
	unlink(sockname);
	return 0;
}
