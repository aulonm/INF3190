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
	addr = argv[3];

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
			if(recvd < 0){
				perror("read");
				return -7;
			}
			printf("Recieved %zd bytes from client %s\n", cfd, rbuf);
			
			//MIP PACKET
			size_t mipsize = sizeof(struct mip_packet)+recvd-1;
			struct mip_packet *mip = malloc(mipsize);
			assert(mip);

			mip->TRA = 4;
			mip->TTL = 16;
			mip->payload = recvd-1;
			mip->dst_addr = 1;
			mip->src_addr = &addr;
			memcpy(mip->contents, rbuf+1, sizeof(rbuf)-1);


			//ETHER FRAME
			size_t msgsize = sizeof(struct ether_frame)+strlen(interface);
			struct ether_frame *frame = malloc(msgsize);

			memcpy(frame->dst_addr, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);

			memcpy(frame->src_addr, iface_hwaddr, 6);

			frame->eth_proto[0] = frame->eth_proto[1] = 0xFF;

			memcpy(frame->contents, mip, sizeof(mip));


			ssize_t retv = send(sock2, frame, msgsize, 0);
			printf("Message size=%zu, sent=%zd\n", msgsize, retv);


			//send(cfd, "Pong!", 5, 0);
		}
		// IF RAW SOCKET
		if(FD_ISSET(sock2, &rdfds)){
			

			char buf[1500];
			struct ether_frame *frame = (struct ether_frame*)buf;
			
			ssize_t retv = recv(sock2, buf, sizeof(buf), 0);
			struct mip_packet *mipp = frame->contents;

			printf("Destination address: ");
			printf("%u\n",mipp->TRA);

		}


	}
	close(sock1);
	close(sock2);
	unlink(sockname);
}