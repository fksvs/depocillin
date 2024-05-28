#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "network.h"
#include "output.h"
#include "type.h"
#include "ip.h"
#include "usage.h"
#include "helpers.h"

static unsigned char *src_addr = NULL, *dst_addr = NULL;
static unsigned char ttl, service = 0;
static int count = 1, verbose = 0;
static char *iface = NULL;

void build_ip(char *buffer, size_t payload_size, unsigned char *src,
	      unsigned char *dst, unsigned char ttl, unsigned char service,
	      unsigned char protocol)
{
	ip_hdr *iph = (ip_hdr *)buffer;
	int protocol_size = 0;

	switch (protocol) {
	case IPPROTO_ICMP:
		protocol_size = sizeof(icmp_hdr);
		break;
	case IPPROTO_TCP:
		protocol_size = sizeof(tcp_hdr);
		break;
	case IPPROTO_UDP:
		protocol_size = sizeof(udp_hdr);
		break;
	}
	iph->length = sizeof(ip_hdr) + protocol_size + payload_size;

	iph->ver_ihl = 0x45;
	iph->service = service;
	iph->ident = htons(getpid());
	iph->frag = 0x00;
	iph->ttl = (ttl) ? ttl : DEFAULT_TTL;
	iph->protocol = protocol;
	iph->check = 0;
	if (!src) {
		struct in_addr addr = { get_address() };
		inet_pton(AF_INET, (const char *)inet_ntoa(addr), &iph->src);
	} else {
		inet_pton(AF_INET, (const char *)src, &iph->src);
	}
	inet_pton(AF_INET, (const char *)dst, &iph->dst);
	iph->check = checksum((unsigned short *)iph, iph->length);
}

static void validate_ip()
{
	if (!dst_addr) {
		err_exit("destination address not specified.");
	}
}

static void usage()
{
	general_usage();
	ip_usage();
	fprintf(stderr, "\n");

	exit(EXIT_FAILURE);
}

static void parser(int argc, char *argv[])
{
	int opt;

	if (argc < 3) {
		usage();
	}

	while ((opt = getopt(argc, argv, "i:c:vhS:D:T:o:")) != -1) {
		switch (opt) {
		case 'i':
			iface = optarg;
			break;
		case 'c':
			count = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
			usage();
			break;
		case 'S':
			src_addr = (unsigned char *)optarg;
			break;
		case 'D':
			dst_addr = (unsigned char *)optarg;
			break;
		case 'T':
			ttl = atoi(optarg);
			break;
		case 'o':
			service = atoi(optarg);
			break;
		case '?':
			break;
		}
	}
}

void inject_ip(int argc, char *argv[])
{
	char buffer[BUFF_SIZE];
	struct sockaddr_in sock_dst;
	int sockfd;

	parser(argc, argv);

	memset(buffer, 0, BUFF_SIZE);
	memset(&sock_dst, 0, sizeof(struct sockaddr_in));

	if ((sockfd = init_socket()) == -1) {
		exit(EXIT_FAILURE);
	}

	validate_ip();

	if (iface) {
		bind_iface(sockfd, iface);
	}

	sock_dst.sin_family = AF_INET;
	inet_pton(AF_INET, (const char *)dst_addr, &sock_dst.sin_addr.s_addr);

	build_ip(buffer, 0, src_addr, dst_addr, ttl, service, 0);

	ip_hdr *iph = (ip_hdr *)buffer;
	send_raw(sockfd, buffer, iph->length, &sock_dst, count);

	if (verbose) {
		ip_hdr *iph = (ip_hdr *)buffer;
		print_ip(iph);
	}

	close_sock(sockfd);
	exit(EXIT_SUCCESS);
}
