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
	printf("%02x\n:", mac[5]);
}

int main(int argc, char *argv[]) {
	int clientSocket, networkSocket, i;
	int debug = 0;							// Debug mode
	uint8_t iface_hwaddr[6];				// Mac address array
	uint8_t arp_cache[256][6];              // ARP/-cache to hold the MAC addrs
	uint8_t arp_check[256];                 // ARP-check to know if there are any addrs there
	uint8_t addr;					// Addres where to send messages
	const char *sockname;					// Socket name
	const char *interface;					// The interface to send packets to
	

	if (argc < 4 || argc > 5) {
		printf("USAGE: %s [socket] [interface] [src-addr] [-d]\n", argv[0]);
		printf("Socket: Socket to connect to\n");
		printf("Interface: The interface to send packets on\n");
		printf("Src-addr: The address you want to send messages to\n");
		printf("-d: Debug mode\n");
		return -2;
	}

	/* Set the mandatory arguments to its respective variable */
	sockname = argv[1];
	interface = argv[2];
	addr = (uint8_t)strtoul(argv[3], 0, 10);

	/* Checks if there are any optional command-line args */
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-d") == 0) {
			debug = 1;
		}
	}

	/* Reset the arrays to 0 */
	memset(&arp_cache, 0, sizeof(arp_cache));
	memset(&arp_check, 0, sizeof(arp_check));

	/**
	*   AF_UNIX         = Local communication
	*   SOCK_SEQPACKET  = Reliable two-way connection
	*   0               =
	**/
	clientSocket = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	/**
	* AF_PACKET = raw socket interface
	* SOCK_RAW  = we cant the 12 header intact (SOCK_DGRAM removes header)
	* ETH_P_ALL = all ethernet protocols
	**/
	networkSocket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_MIP));

	/* If the clientsocket returns -1 there is a problem */
	if (clientSocket == -1) {
		perror("ClientSocket");
		return -3;
	}

	/* If the networksSocket returns -1 there is a problem */
	if (networkSocket == -1) {
		perror("raw socket");
		return -4;
	}

	/* Gets the MAC address, and if the return value is not 0, exit */
	if (get_if_hwaddr(networkSocket, interface, iface_hwaddr) != 0) {
		return -5;
	}

	/* Print the hardware address of the interface */
	printf("HW-addr: ");
	for (i = 0; i < 5; ++i) {
		printf("%02x:", iface_hwaddr[i]);
	}
	printf("%02x\n", iface_hwaddr[5]);

	// Bind the networksocket to the specified interface
	struct sockaddr_ll device;
	memset(&device, 0, sizeof(device));
	device.sll_family = AF_PACKET;
	device.sll_ifindex = if_nametoindex(interface);

	if (bind(networkSocket, (struct sockaddr *)&device, sizeof(device))) {
		perror("Could not bind raw socket");
		close(networkSocket);
		return -6;
	}

	// Bind the AF_UNIX socket
	struct sockaddr_un bindaddr;
	bindaddr.sun_family = AF_UNIX;
	strncpy(bindaddr.sun_path, sockname, sizeof(bindaddr.sun_path));
	int retv = bind(clientSocket, (struct sockaddr *)&bindaddr, sizeof(bindaddr.sun_path));

	/* If it doesnt return 0, it couldnt bind to the socket */
	if (retv != 0) {
		perror("bind");
		return -4;
	}

	/* Listens to incoming connections from client programs */
	if (listen(clientSocket, 5)) {
		perror("listen");
		return -5;
	}

	fd_set readfds;
	int cfd = -1;
	while (1) {
		/* Clear the set ahead of time */
		FD_ZERO(&readfds);

		/* If the cfd is less then 0, add a clientSocket to the set */
		if (cfd < 0) {
			printf("Setting socket\n");
			FD_SET(clientSocket, &readfds);
		}
		/* Else add the cfd to the set */
		else {
			printf("Setting cfd\n");
			FD_SET(cfd, &readfds);
		}

		/* Add the networkSocket to the set */
		FD_SET(networkSocket, &readfds);
		int maxfd;

		/* Finds out the maxvalue of the file descriptors */
		if (cfd > maxfd) maxfd = cfd;
		if (clientSocket > maxfd) maxfd = clientSocket;
		if (networkSocket > maxfd) maxfd = networkSocket;

		printf("Waiting for select\n");
		retv = select(maxfd + 1, &readfds, NULL, NULL, NULL);
		
		if (retv <= 0) {
			perror("select");
			return -6;
		}

		/* If you get a cfd then this part happens, the client is sending messages */
		printf("Before select\n");
		if (FD_ISSET(cfd, &readfds)) {
			char rbuf[1500];
			memset(rbuf, 0, sizeof(rbuf));
			ssize_t recvd = recv(cfd, rbuf, sizeof(rbuf), 0);

			if (recvd == 0) {
				cfd = -1;
				continue;
			} else if (recvd < 0) {
				perror("read");
				return -7;
			} else {
				printf("Recieved %zd bytes from client %s\n", cfd, rbuf);
			}

			/* If the arp cache table doesnt have the address, send an arp request */
			/* ARP REQUEST */
			if (arp_check[(uint8_t)rbuf[0]] != 1) {
				/* Start making the Ether frame */
				size_t mipsize = sizeof(struct mip_packet) + recvd;
				struct mip_packet *arp_req = malloc(mipsize);
				assert(arp_req);

				arp_req->TRA = 1;										// TRA = 4 == 100
				arp_req->TTL = 15;										// Payload
				arp_req->payload = 0;									// Destination addr
				arp_req->src_addr = addr;								// Source addr
				arp_req->dst_addr = (uint8_t)rbuf[0];

				size_t fsize = sizeof(struct ether_frame) + mipsize;
				struct ether_frame *frame = malloc(fsize);

				/* Ethernet destination address */
				memcpy(frame->dst_addr, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
				
				/* Fill in our source address */
				memcpy(frame->src_addr, iface_hwaddr, 6);

				/* Ethernet protocol field = 0xFFFF (MIP) */
				frame->eth_proto[0] = frame->eth_proto[1] = 0xFF;

				/* Fill in the message */
				memcpy(frame->contents, arp_req, mipsize);
				
				/* Send the packet */
				ssize_t retv = send(networkSocket, frame, fsize, 0);
				
				if(retv < 0){
					perror("ARP Request send");
					return -20;
				}

				printf("Message size=%zu, sent=%zd\n", fsize, retv);

				/* Start waiting for an ARP Response */
				while (1) {

					char buf[1500];
					memset(buf, 0, sizeof(buf));
					struct ether_frame *arp_resp_frame = (struct ether_frame *)buf;
					ssize_t retv = recv(networkSocket, buf, sizeof(buf), 0);

					if(retv < 0){
						perror("Receive");
						return -15;
					}

					struct mip_packet *arp_resp_mip = (struct mip_packet *)arp_resp_frame->contents;

					/* If TRA bits are 000 then it is an ARP Response */
					if ((arp_resp_mip->TRA == 0) && (arp_resp_mip->dst_addr == addr)) {
						/* Cache the MAC address */
						memcpy(arp_cache[arp_resp_mip->src_addr], arp_resp_frame->src_addr, 6);
						arp_check[arp_resp_mip->src_addr] = 1;
						break;
					}
				}
			}

			/**
			* Now the arp cache table should have the addres
			* and we can continue to send our message to the other mip 
			* Transport happens from here
			* First we make our MIP packet:
			**/
			size_t mipsize = sizeof(struct mip_packet) + recvd;
			struct mip_packet *mip = malloc(mipsize);
			assert(mip);
			mip->TRA = 4;										
			mip->TTL = 15;										
			mip->payload = recvd - 1;							
			mip->dst_addr = (uint8_t)rbuf[0];					
			mip->src_addr = addr;								
			memcpy(mip->contents, rbuf, sizeof(rbuf));	// Copy buf to contents

			/* Start making the ether frame */
			size_t msgsize = sizeof(struct ether_frame) + mipsize;
			struct ether_frame *frame = malloc(msgsize);
			memcpy(frame->dst_addr, arp_cache[(uint8_t)rbuf[0]], 6);
			
			memcpy(frame->src_addr, iface_hwaddr, 6);

			frame->eth_proto[0] = frame->eth_proto[1] = 0xFF;

			memcpy(frame->contents, mip, mipsize);

			ssize_t retv = send(networkSocket, frame, msgsize, 0);

			if(retv < 0){
				perror("Send");
				return -16;
			}

			//printf("Message size=%zu, sent=%zd\n", msgsize, retv);
		}

		/* If we dont have any connection on that socket, then accept one */
		if (FD_ISSET(clientSocket, &readfds)) {
			cfd = accept(clientSocket, NULL, NULL);
		}

		/* If you get a networkSocket then this will happen */
		if (FD_ISSET(networkSocket, &readfds)) {

			/* Receive the ethernet frame from an other MIP */
			char buf[1500];
			struct ether_frame *frame = (struct ether_frame *)buf;
			
			ssize_t retv = recv(networkSocket, buf, sizeof(buf), 0);

			if(retv < 0){
				perror("Network receive");
				return -19;
			}

			struct mip_packet *recv_mip = (struct mip_packet *)frame->contents;

			/* Check if it is a transport and destination is this mip-daemon */
			if ((recv_mip->TRA == 4) && (recv_mip->dst_addr == (uint8_t)addr)) {
				char sbuf[1500];
				memset(sbuf, 0, sizeof(sbuf));
				sbuf[0] = recv_mip->src_addr;
				strcat(sbuf, recv_mip->contents);

				/* Send message to server */
				printf("Sending...\n");
				ssize_t sent = send(cfd, sbuf, strlen(sbuf), 0);
				
				if(sent < 0){
					perror("Send");
					return -17;
				}
				
				printf("Sent: %d\n", sent);
			} 
			/* Check if it is an ARP Request, if it is then start making ARP Response and send */
			else if ((recv_mip->TRA == 1) && (recv_mip->dst_addr == addr)) { 
				size_t resp_mipsize = sizeof(struct mip_packet) + retv;
				struct mip_packet *resp_packet = malloc(resp_mipsize);
				assert(resp_packet);

				resp_packet->TRA = 0;								// TRA = 0 = 000				
				resp_packet->TTL = 15;
				resp_packet->src_addr = recv_mip->dst_addr;
				resp_packet->dst_addr = recv_mip->src_addr;

				size_t resp_fsize = sizeof(struct ether_frame) + resp_mipsize;
				struct ether_frame *resp_frame = malloc(resp_fsize);

				memcpy(resp_frame->dst_addr, frame->src_addr, 6);
				memcpy(resp_frame->src_addr, iface_hwaddr, 6);
				resp_frame->eth_proto[0] = resp_frame->eth_proto[1] = 0xFF;
				memcpy(resp_frame->contents, resp_packet, resp_mipsize);
				puts("ARP response sending!\n");
				ssize_t send_resp = send(networkSocket, resp_frame, resp_fsize, 0);
				
				if(send_resp < 0){
					perror("ARP Response send");
					return -18;
				}

				// CACHE THE ADDRESS TO ARP CACHE TABLE
				memcpy(arp_cache[recv_mip->src_addr], frame->src_addr, 6);
				arp_check[recv_mip->src_addr] = 1;
			}
		}
	}
	close(clientSocket);
	close(networkSocket);
	unlink(sockname);
}