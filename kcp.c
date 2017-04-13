#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include "ikcp/ikcp.h"

int sock_create(char *ip, unsigned short port)
{
	int sockfd;
	int yes = 1;
	struct sockaddr_in addr;
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		perror("reuseaddr");
                exit(1);
	}

	/* init servaddr */
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);
	/* bind address and port to socket */
	if(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		perror("bind error");
		exit(1);
	}
	return sockfd;
}

static inline void itimeofday(long *sec, long *usec)
{
	#if defined(__unix)
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
	#else
	static long mode = 0, addsec = 0;
	BOOL retval;
	static IINT64 freq = 1;
	IINT64 qpc;
	if (mode == 0) {
		retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		freq = (freq == 0)? 1 : freq;
		retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
		addsec = (long)time(NULL);
		addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
		mode = 1;
	}
	retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
	retval = retval * 2;
	if (sec) *sec = (long)(qpc / freq) + addsec;
	if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
	#endif
}

static inline IINT64 iclock64(void)
{
	long s, u;
	IINT64 value;
	itimeofday(&s, &u);
	value = ((IINT64)s) * 1000 + (u / 1000);
	return value;
}

static inline IUINT32 iclock()
{
	return (IUINT32)(iclock64() & 0xfffffffful);
}

#define MTU 1400
int peer_fd;
struct sockaddr_in peer_addr;
int peer_valid;

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	char xbuf[2048];
	xbuf[0] = 0;
	memcpy(xbuf + 1, buf, len);
	return sendto(peer_fd, xbuf, len + 1, 0, (struct sockaddr *)&peer_addr,
		      sizeof(struct sockaddr_in));
}

int main(int argc, char **argv)
{
	char buf[MTU+1];

	if(argc != 4)
		exit(1);
	if(argv[1][0] == 'c') {
		peer_fd = sock_create("0.0.0.0", 0);
		bzero(&peer_addr, sizeof(peer_addr));
		peer_addr.sin_family = AF_INET;
		peer_addr.sin_addr.s_addr = inet_addr(argv[2]);
		peer_addr.sin_port = htons(atoi(argv[3]));
		peer_valid = 1;
		buf[0] = 0xaa;
		sendto(peer_fd, buf, 1, 0, (struct sockaddr *)&peer_addr,
		       sizeof(struct sockaddr_in));
	} else {
		peer_fd = sock_create(argv[2], atoi(argv[3]));
		peer_valid = 0;
	}

	ikcpcb *kcp = ikcp_create(0x11223344, 0);
	ikcp_setoutput(kcp, udp_output);
	ikcp_wndsize(kcp, 128, 128);
	//ikcp_nodelay(kcp, 0, 10, 0, 0);  // normal
	ikcp_nodelay(kcp, 0, 10, 0, 1);  // standard
	//ikcp_nodelay(kcp, 1, 10, 2, 1);  // fast
	//kcp->rx_minrto = 10;             //
	//kcp->fastresend = 1;             //

	fd_set rfds;
	int ret;
	struct timeval to;
	int n;

	while (1) {
		ikcp_update(kcp, iclock());
		while (1) {
			n = ikcp_recv(kcp, buf, MTU);
			if(n < 0) break;
			if(write(1, buf, n) <= 0)
				goto out;
		}

		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
		FD_SET(peer_fd, &rfds);
		to.tv_sec = 0;
		to.tv_usec = 5 * 1000;

		ret = select(peer_fd + 1, &rfds, NULL, NULL, &to);
		if(ret == -1) {
			perror("select");
			exit(1);
		} else if(ret) {
			if(FD_ISSET(peer_fd, &rfds)) {
				// peer_fd --> 1
				socklen_t alen = sizeof(struct sockaddr_in);
				n = recvfrom(peer_fd, buf, MTU + 1, 0,
					     (struct sockaddr *)&peer_addr, &alen);
				peer_valid = 1;
				if(n <= 0)
					goto out;

				if(buf[0] == 0) {
					ikcp_input(kcp, buf + 1, n - 1);
				}
			}
			if(FD_ISSET(0, &rfds)) {
				// 0 --> peer_fd
				if(peer_valid) {
					int n = read(0, buf, MTU);
					if (n <= 0)
						goto out;
					ikcp_send(kcp, buf, n);
				}
			}
		}
	}
out:
	return 0;
}
