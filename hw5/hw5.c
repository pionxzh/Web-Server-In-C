#include <stdio.h>
#include <string.h>
#include <pcap.h>
#include <time.h>
#include <arpa/inet.h>

#define ETHERTYPE_IPv4 0x0800
#define ETHERTYPE_IPv6 0x086dd
#define PROTOCAL_TCP 6
#define PROTOCAL_UDP 17

//ethernet header
typedef struct eth_header {
	u_char dst_mac[6];
	u_char src_mac[6];
	u_short eth_type;
}eth_header;
eth_header *ethernet;

//ipv4 header
typedef struct ip_header {
	int version:4;
	int header_len:4;
	u_char tos:8;
	int total_len:16;
	int ident:16;
	int flags:16;
	u_char ttl:8;
	u_char protocol:8;
	int checksum:16;
	u_char sourceIP[4];
	u_char destIP[4];
}ip_header;
ip_header *ip;

//ipv6 header
typedef struct ip6_header{
	unsigned int version:4;
	unsigned int traffic_class:8;
	unsigned int flow_label:20;
	uint16_t payload_len;
	uint8_t next_header;
	uint8_t hop_limit;
	uint16_t sourceIP[8];
	uint16_t destIP[8];
}ip6_header;
ip6_header *ip6;

//tcp header
typedef struct tcp_header {
	u_short sport;
	u_short dport;
	u_int seq;
	u_int ack;
	u_char head_len;
	u_char flags;
	u_short wind_size;
	u_short check_sum;
	u_short urg_ptr;
}tcp_header;
tcp_header *tcp;

//udp header
typedef struct udp_header {
	u_short sport;
	u_short dport;
	u_short tot_len;
	u_short check_sum;
}udp_header;
udp_header *udp;

void packet_handler(u_char *arg, const struct pcap_pkthdr *pkt_header, const u_char *pkt_data) {
	static int count = 0;
	count++;

	printf("> Packet #%d\n\n",count);
	printf("TimeStamp: %s",ctime((const time_t*)&pkt_header->ts.tv_sec));//get packet time
	printf("Content length: %d\n", pkt_header->len);
	//length of header
	u_int eth_len=sizeof(struct eth_header);
	u_int ip_len=sizeof(struct ip_header);
	u_int ip6_len=sizeof(struct ip6_header);

	ethernet=(eth_header *)pkt_data;
	//decided the type of protocal
	if(ntohs(ethernet->eth_type) == ETHERTYPE_IPv4) {
		printf("Ethernet Type: IPv4\n\n");
		printf("Header:\n");
		ip=(ip_header*)(pkt_data+eth_len);
		printf("source ip: %d.%d.%d.%d\n",ip->sourceIP[0],ip->sourceIP[1],ip->sourceIP[2],ip->sourceIP[3]);
		printf("dest   ip: %d.%d.%d.%d\n",ip->destIP[0],ip->destIP[1],ip->destIP[2],ip->destIP[3]);
		if(ip->protocol == PROTOCAL_TCP) {
			tcp=(tcp_header*)(pkt_data+eth_len+ip_len);
			printf("protocol:  TCP\n");
			printf("source port: %u\n",htons(tcp->sport));
			printf("dest   port: %u\n",htons(tcp->dport));
		}
		else if(ip->protocol == PROTOCAL_UDP) {
			udp=(udp_header*)(pkt_data+eth_len+ip_len);
			printf("protocol:  UDP\n");
			printf("source port: %u\n",htons(udp->sport));
			printf("dest   port: %u\n",htons(udp->dport));
		 }
		else {
			printf("protocol:  UNKNOWN\n");
		}
	}
	else if(ntohs(ethernet->eth_type) == ETHERTYPE_IPv6) {
		printf("Ethernet Type: IPv6\n");
        printf("Header:\n");
		ip6=(ip6_header*)(pkt_data+eth_len);
		char str[INET6_ADDRSTRLEN];
		printf("source ip6:%s\n",inet_ntop(AF_INET6,ip6->sourceIP,str,sizeof(str)));
		printf("dest ip6:%s\n",inet_ntop(AF_INET6,ip6->destIP,str,sizeof(str)));
        if(ip6->next_header == PROTOCAL_TCP) {
			tcp=(tcp_header*)(pkt_data+eth_len+ip6_len);
			printf("protocol:  TCP\n");
			printf("source port: %u\n",htons(tcp->sport));
			printf("dest   port: %u\n",htons(tcp->dport));
        }
        else if(ip6->next_header == PROTOCAL_UDP) {
			udp=(udp_header*)(pkt_data+eth_len+ip6_len);
			printf("protocol:  UDP\n");
			printf("source port: %u\n",htons(udp->sport));
			printf("dest   port: %u\n",htons(udp->dport));
            }
        else {
			printf("protocol:  UNKNOWN\n");
        }
	}
	else{
		printf("Ethernet Type: UNKNOWN\n");
	}
	printf("\n========================================\n\n");
}

int main(int argc,char *argv[]) {
    char *condition = "any";
    bpf_u_int32 net;
    bpf_u_int32 mask;

    pcap_t *handle;
    struct bpf_program filter;
    char errbuf[PCAP_ERRBUF_SIZE];

    if (argc == 1) {
        return printf("Please follow the format 〔 ./hw5 [pcapFile] [filter] 〕\n");
    }

    if (argc == 3) {
        condition = argv[2];
    }
    printf("Filename: %s\nFilter: %s\n\n", argv[1], condition);

	handle = pcap_open_offline(argv[1], errbuf);
    pcap_compile(handle, &filter, argv[2], 0, net);
    pcap_setfilter(handle, &filter);
    pcap_loop(handle,-1, packet_handler, NULL);
    pcap_close(handle);

    return 0;
}
