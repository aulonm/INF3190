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
 * Struct definition of an MIP Packet
 */	
struct mip_packet{
	unsigned int TRA:3;
	unsigned int TTL:4;
	unsigned int payload:9;
	uint8_t src_addr;
	uint8_t dst_addr;
	char	contents[0];
} __attribute__((packed));

/**
 * Struct definition of an ethernet frame
 */
struct ether_frame{
	uint8_t dst_addr[6];
	uint8_t src_addr[6];
	uint8_t eth_proto[2];
	uint8_t contents[0];
} __attribute__((packed));

/**
*	Retrieves the hardware address of the given network device
*
*	@param sock 	Socket to use for the IOCTL
* 	@param devname 	Name of the network device (for example eth0)
* 	@param hwaddr 	Buffer to write the hardware address to
* 	@param Zero on succes, -1 otherwise
**/
static int get_if_hwaddr(int sock, const char* devname, uint8_t hwaddr[6]){
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));

	assert(strlen(devname) < sizeof(ifr.ifr_name));
	strcpy(ifr.ifr_name, devname);

	if(ioctl(sock, SIOCGIFHWADDR, &ifr) < 0){
		perror("ioctl");
		return -1;
	}

	memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, 6*sizeof(uint8_t));

	return 0;
}

static void printmac(uint8_t mac[6]){
	int i;
	for(i = 0; i < 5; i++){
		printf("%02x:", mac[i]);
	}
	printf("%02x\n:", mac[5]);
}




int main(int argc, char* argv[]){
	int sock1, sock2, i;
	const char* sockname;
	uint8_t iface_hwaddr[6];
	const char* interface;
	const uint8_t* addr;
	uint8_t arp_cache[256][6];				// ARP/-cache to hold the MAC addrs
	uint8_t arp_check[256];					// ARP-check to know if there are any addrs there

	if(argc != 4){
		printf("USAGE: %s [socket] [interface] [src-addr]\n", argv[0]);
		printf("interface: The interface to send packets on\n");
		return -2;
	}

	// nuller ut arraysene
	memset(&arp_cache, 0, sizeof(arp_cache));
	memset(&arp_check, 0, sizeof(arp_check));


	sockname = argv[1];
	interface = argv[2];
	addr = strtoul(argv[3], 0, 10);

	sock1 = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	
	/** 
	* AF_PACKET = raw socket interface
	* SOCK_RAW 	= we cant the 12 header intact (SOCK_DGRAM removes header)
	* ETH_P_ALL = all ethernet protocols
	**/
	sock2 = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_MIP));


	if(sock1 == -1){
		perror("socket");
		return -3;
	}

	if(sock2 == -1){
		perror("raw socket");
		return -4;
	}

	if(get_if_hwaddr(sock2, interface, iface_hwaddr) != 0){
		return -5;
	}


	// Print the hardware address of the interface
	printf("HW-addr: ");
	for(i = 0; i < 5; ++i){
		printf("%02x:", iface_hwaddr[i]);
	}
	printf("%02x\n", iface_hwaddr[5]);


	// Bind the socket to the specified interface 
	struct sockaddr_ll device;
	memset(&device, 0, sizeof(device));

	device.sll_family = AF_PACKET;
	device.sll_ifindex = if_nametoindex(interface);

	if(bind(sock2, (struct sockaddr*)&device, sizeof(device))){
		perror("Could not bind raw socket");
		close(sock2);
		return -6;
	}

	// Bind the AF_UNIX socket
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
	int cfd = -1;
	while(1){
		
		FD_ZERO(&rdfds);

		if(cfd < 0){
			printf("Setting socket\n");
			FD_SET(sock1, &rdfds);
		}else{
			printf("Setting cfd\n");
			FD_SET(cfd, &rdfds);
		}
		// if(cfd == 0){
		// 	cfd = accept(sock1, NULL, NULL);
		// 	printf("server connect\n");
		// }

		//FD_SET(cfd, &rdfds);
		FD_SET(sock2, &rdfds);
		
		int maxfd;

		if(cfd > maxfd) maxfd = cfd;
		if(sock1 > maxfd) maxfd = sock1;
		if(sock2 > maxfd) maxfd = sock2;

		//int maxfd = sock2;

		printf("Waiting for select\n");
		retv = select(maxfd+1, &rdfds, NULL, NULL, NULL);
		if(retv <= 0){
			perror("select");
			return -6;
		}


		// IF AF_UNIX SOCKET
		printf("Before select\n");
		if(FD_ISSET(cfd, &rdfds)){
			//cfd = accept(sock1, NULL, NULL);
			printf("New connection local! %d\n", cfd);
			char rbuf[100];
			ssize_t recvd = recv(cfd, rbuf, 99, 0);
			if(recvd == 0){
				// perror("read");
				// return -7;
				//close(cfd);
				cfd = -1;
				continue;
			}else if(recvd < 0){
				perror("read");
				return -7;
			}else{
				printf("Recieved %zd bytes from client %s\n", cfd, rbuf);
			}

			printf("THIS IS THE ADDRESS FOR ARP: %u\n", (uint8_t)rbuf[0]);

			if(arp_check[(uint8_t)rbuf[0]] != 1){
				printf("I DONT HAVE THIS ADDRESS\n");
			}
			
			// ARP REQUEST
			if(arp_check[(uint8_t)rbuf[0]] != 1){
				printf("SENDING ARP REQUEST:\n");
				size_t mipsize = sizeof(struct mip_packet) + recvd-1;
				struct mip_packet *arp_req = malloc(mipsize);
				assert(arp_req);

				arp_req->TRA = 1;
				arp_req->TTL = 15;
				arp_req->payload = 0;
				arp_req->src_addr = addr;
				arp_req->dst_addr = (uint8_t)rbuf[0];

				size_t fsize = sizeof(struct ether_frame) + mipsize;
				struct ether_frame *frame = malloc(fsize);

				memcpy(frame->dst_addr, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
				memcpy(frame->src_addr, iface_hwaddr, 6);
				frame->eth_proto[0] = frame->eth_proto[1] = 0xFF;
				memcpy(frame->contents, arp_req, mipsize);
				ssize_t retv = send(sock2, frame, fsize, 0);
				printf("Message size=%zu, sent=%zd\n", fsize, retv);

				while(1){
					printf("WAITING FOR ARP RESPONSE\n");
					char buf[1500];
					struct ether_frame *arp_resp_frame = (struct ether_frame*)buf;
			
					ssize_t retv = recv(sock2, buf, sizeof(buf), 0);
					struct mip_packet *arp_resp_mip = (struct mip_packet*)arp_resp_frame->contents;
					// IF TRA BITS ARE 000 THEN IT IS AN ARP RESPONSE
					if((arp_resp_mip->TRA == 0) && (arp_resp_mip->dst_addr == (uint8_t)addr)){
						// CACHE 
					}else{
						printf("This is not the response I was looking for");
					}

				}

			}

			//MIP PACKET
			size_t mipsize = sizeof(struct mip_packet)+recvd-1;
			struct mip_packet *mip = malloc(mipsize);
			//printf("mipsize: %d", mipsize);
			assert(mip);

			mip->TRA = 4;
			mip->TTL = 15;
			mip->payload = recvd-1;
			mip->dst_addr = 1;
			mip->src_addr = &addr;
			// printf("TRA: %d", mip->TRA);
			// printf("TTL: %d", mip->TTL);
			// printf("Payload: %d", mip->payload);
			// printf("sent buf: %s\n", rbuf);
			memcpy(mip->contents, rbuf+1, sizeof(rbuf)-1);

			//printf("mip contents: %s\n", mip->contents);


			//ETHER FRAME
			size_t msgsize = sizeof(struct ether_frame) + mipsize;
			struct ether_frame *frame = malloc(msgsize);

			memcpy(frame->dst_addr, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);

			memcpy(frame->src_addr, iface_hwaddr, 6);

			frame->eth_proto[0] = frame->eth_proto[1] = 0xFF;

			memcpy(frame->contents, mip, mipsize);

			ssize_t retv = send(sock2, frame, msgsize, 0);
			printf("Message size=%zu, sent=%zd\n", msgsize, retv);


			//send(cfd, "Pong!", 5, 0);
		}
		if(FD_ISSET(sock1, &rdfds)){
			printf("Connected sock\n");
			cfd = accept(sock1, NULL, NULL);
		}
		// IF RAW SOCKET
		if(FD_ISSET(sock2, &rdfds)){
			
			// GET THE ETHERNET FRAME FROM OTHER MIPS
			char buf[1500];
			struct ether_frame *frame = (struct ether_frame*)buf;
			
			ssize_t retv = recv(sock2, buf, sizeof(buf), 0);
			struct mip_packet *mipp = (struct mip_packet*)frame->contents;

			// Check if it is a transport and destination is this mip-daemon
			if((mipp->TRA == 4) && (mipp->dst_addr == (uint8_t)addr)){

			}else if((mipp->TRA == 1) && (mipp->dst_addr == (uint8_t)addr)){ 	//check if it is a arp request and destination is this mip-daemon
				// Make a new MIP packet with an ARP RESPONSE
				puts("Got ARP request, now making an ARP response\n");
				size_t resp_mipsize = sizeof(struct mip_packet) + retv; // is it possible to use the size of recieved mipp?
				struct mip_packet *resp_packet = malloc(resp_mipsize);
				assert(resp_packet);

				resp_packet->TRA = 0;
				resp_packet->TTL = 15;
				resp_packet->src_addr = mipp->dst_addr;
				resp_packet->dst_addr = mipp->src_addr;

				size_t resp_fsize = sizeof(struct ether_frame) + resp_mipsize;
				struct ether_frame *resp_frame = malloc(resp_fsize);

				memcpy(resp_frame->dst_addr, frame->src_addr, 6);
				memcpy(resp_frame->src_addr, iface_hwaddr, 6);
				resp_frame->eth_proto[0] = resp_frame->eth_proto[1] = 0xFF;
				memcpy(resp_frame->contents, resp_packet, resp_mipsize);
				puts("ARP response sending!\n");
				ssize_t send_resp = send(sock2, resp_frame, resp_fsize, 0);
				// CACHE THE ADDRESS TO ARP CACHE TABLE
				memcpy(arp_cache[mipp->src_addr], frame->src_addr, 6);

			}else{ // else just fuck off and drop it
				printf("This is not the mip you are looking for\n");
			}

			printf("Destination address: ");
			printf("%s\n",mipp->contents);

			char* sbuf[100];
			sbuf[0] = mipp->src_addr;
			printf("%s\n", sbuf);
			strcat(sbuf, mipp->contents);

			// SEND MESSAGE TO SERVER
			printf("Sending...\n");
			ssize_t sent = send(cfd, sbuf, strlen(sbuf), 0);
			printf("Sent: %d\n", sent);
		}


	}
	close(sock1);
	close(sock2);
	unlink(sockname);
}