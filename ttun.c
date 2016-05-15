#include <stdio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>

#define MTU 1400
#define BUF_SIZE 4096

int tun_create(char *dev, int flags)
{
	struct ifreq ifr;
	int fd, err;

	if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
		return fd;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags |= flags;
	if (*dev != '\0')
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
		close(fd);
		return err;
	}
	strcpy(dev, ifr.ifr_name);

	return fd;
}

void readn(int fd, void *buf, int n)
{
	int ret;
	int last = n;
	int p = 0;
	while(last) {
		ret = read(fd, buf + p, last);
		if(ret <= 0) {
			perror("readn");
			exit(1);
		}
		last -= ret;
		p += ret;
	}
}

void writen(int fd, void *buf, int n)
{
	int ret;
	int last = n;
	int p = 0;
	while(last) {
		ret = write(fd, buf + p, last);
		if(ret <= 0) {
			perror("writen");
			exit(1);
		}
		last -= ret;
		p += ret;
	}
}

int check_pack(unsigned char *buf, int n)
{
	int i;
	unsigned char x;
	if(n == 4 && buf[0] == 0xb && buf[1] == 0xf)
		return 1;
	if(n > 4 && buf[0] == 0xd)
	{
		x = 0;
		for(i = 0; i < n; i++)
			x ^= buf[i];
		if(x == 0)
			return 0;
	}
	return -1;
}

void make_pack(unsigned char *buf, int n)
{
	int i;
	unsigned char x;
	buf[0] = 0xd;
	buf[1] = 0;
	buf[2] = (n >> 8) & 0xff;
	buf[3] = (n & 0xff);
	x = 0;
	for(i = 0; i < n; i++)
		x ^= buf[i];
	buf[1] = x;
}

void make_pack_ping(unsigned char *buf)
{
	buf[0] = 0xb;
	buf[1] = 0xf;
	buf[2] = 0;
	buf[3] = 4;
}

static int tun_fd;
static int peer_rfd, peer_wfd;
static time_t peer_last;
static unsigned char buf[BUF_SIZE];

void ping()
{
	time_t t;
	t = time(NULL);

	if(t - peer_last > 30) {
		make_pack_ping(buf);
		writen(peer_wfd, buf, 4);
		peer_last = t;
	}
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

	if(argc != 2)
		return -1;

	peer_rfd = STDIN_FILENO;
	peer_wfd = STDOUT_FILENO;
	peer_last = time(NULL);
		
	tun_name[0] = '\0';
	tun_fd = tun_create(tun_name, IFF_TUN | IFF_NO_PI);
	if (tun_fd < 0) {
		perror("tun_create");
		return 1;
	}

	sprintf(buf, "%s %s %d", argv[1], tun_name, MTU);
	system(buf);
	fprintf(stderr, "TUN name is %s\n", tun_name);

	nfds = (peer_rfd > peer_wfd ? peer_rfd : peer_wfd);
	nfds = (tun_fd > nfds ? tun_fd : nfds);
	nfds++;

	while (1) {
		FD_ZERO(&rfds);
		FD_SET(tun_fd, &rfds);
		FD_SET(peer_rfd, &rfds);
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		ret = select(nfds, &rfds, NULL, NULL, &tv);
		if(ret == -1) {
			perror("select");
			exit(1);
		}
		else if(ret) {
			if(FD_ISSET(peer_rfd, &rfds))
			{
				readn(peer_rfd, buf, 4);
				n = (buf[2] << 8) | buf[3];
				assert(n <= MTU + 4);
				if(n > 4)
					readn(peer_rfd, buf + 4, n - 4);

				switch(check_pack(buf, n))
				{
				case 0:
					write(tun_fd, buf + 4, n - 4);
					/* no break */
				case 1:
					/* update timestamp */
					peer_last = time(NULL);
					break;
				default:
					fprintf(stderr, "error packet: %zd\n", n);
					for(j = 0; j < n; j++)
						fprintf(stderr, "%02x ", buf[j]);
					break;
				}
			}

			if(FD_ISSET(tun_fd, &rfds)) {
				n = read(tun_fd, buf + 4, BUF_SIZE - 4);
				make_pack(buf, n + 4);
				writen(peer_wfd, buf, n + 4);
			}
		}
		ping();
	}
	return 0;
}
