#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>	

// The ethernet device to retrieve the IP of
#define ETHERNET_DEVICE  "eth0"

int main(int argc, char **argv)
{
	int sock;
	struct ifreq ifr;
	struct sockaddr_in *ifaddr;
	
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, ETHERNET_DEVICE, IF_NAMESIZE);
	if (ioctl(sock, SIOCGIFADDR, &ifr) == -1)
	{
		perror("Error retrieving IP address");
	}
	else
	{
	  ifaddr = (struct sockaddr_in *)&ifr.ifr_addr;
	  printf("According to SIOCGIFADDR Device %s has IP-address: %s\n", ETHERNET_DEVICE, inet_ntoa(ifaddr->sin_addr));
	}
	 
	return 0;
}
