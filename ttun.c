#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "comm.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

int tun_fd;
int peer_fd;
time_t peer_last;
struct sockaddr_in peer_addr;
int my_id, peer_id;
unsigned char buf[4096];

void ping()
{
	time_t t;
	t = time(NULL);
	make_pack_ping(buf);
	if(t - peer_last > 30)
		if(write(peer_fd, buf, 4) < 0)
			perror("ping");
}

int main(int argc, char *argv[])
{
	int ret;
	ssize_t n;
	socklen_t alen;
	int nfds;
	char tun_name[IFNAMSIZ];

	fd_set rfds;
	struct timeval tv;
	int j;

	if(argc != 4)
		return -1;
	if(argv[1][0] == 'c') {
		my_id = 1;
		peer_id = 2;
		bzero(&peer_addr, sizeof(peer_addr));
		peer_addr.sin_family = AF_INET;
		peer_addr.sin_addr.s_addr = inet_addr(argv[2]);
		peer_addr.sin_port = htons(atoi(argv[3]));
		if((peer_fd = sock_create("0.0.0.0", 0)) < 0)
			exit(1);
		if(connect(peer_fd, (struct sockaddr *)&peer_addr, sizeof(peer_addr))) {
			perror("connect");
			exit(1);
		}
		printf("connect\n");
	} else {
		int listen_fd;
		my_id = 2;
		peer_id = 1;
		listen_fd = sock_create(argv[2], atoi(argv[3]));
		alen = sizeof(peer_addr);
		listen(listen_fd, 10);
		peer_fd = accept(listen_fd, (struct sockaddr *)&peer_addr, &alen);
		if(peer_fd < 0) {
			perror("accept");
			exit(1);
		}
		close(listen_fd);
		printf("accept\n");
	}

	peer_last = time(NULL);
		
	tun_name[0] = '\0';
	tun_fd = tun_create(tun_name, IFF_TUN | IFF_NO_PI);
	if (tun_fd < 0) {
		perror("tun_create");
		return 1;
	}
	sprintf(buf, "ifconfig %s 192.168.112.%d dstaddr 192.168.112.%d mtu 1400 up", tun_name, my_id, peer_id);
	system(buf);
	sprintf(buf, "./local_config %s", tun_name);
	system(buf);
	printf("TUN name is %s\n", tun_name);

	nfds = (peer_fd > tun_fd ? peer_fd : tun_fd) + 1;

	while (1) {
		FD_ZERO(&rfds);
		FD_SET(tun_fd, &rfds);
		FD_SET(peer_fd, &rfds);
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		ret = select(nfds, &rfds, NULL, NULL, &tv);
		if(ret == -1)
			perror("select");
		else if(ret) {
			if(FD_ISSET(peer_fd, &rfds))
			{
				if(read(peer_fd, buf, 4) != 4) {
					perror("peer_fd");
					exit(1);
				}
				n = (buf[2] << 8) | buf[3];
				if(n > 4) {
					int last = n - 4;
					int p = 4;
					int ret;
					while(last) {
						ret = read(peer_fd, buf + p, last);
						if(ret < 0)
							perror("peer_fd");
						last -= ret;
						p += ret;
					}
				}
				switch(check_pack(buf, n))
				{
				case 0:
					write(tun_fd, buf+4, n-4);
					/* no break */
				case 1:
					/*update timestamp*/
					peer_last = time(NULL);
					break;
				default:
					printf("error packet: %zd\n", n);
					for(j = 0; j < n; j++)
						printf("%02x ", buf[j]);
					break;
				}
			} else if(FD_ISSET(tun_fd, &rfds)) {
				int last, p, ret;
				n = read(tun_fd, buf+4, 4096-4);
				make_pack(buf, n+4);
				last = n + 4;
				p = 0;
				while(last) {
					ret = write(peer_fd, buf + p, last);
					if(ret < 0)
						perror("peer_fd w");
					last -= ret;
					p += ret;
				}
			}
		}
		ping();
	}
	return 0;
}
