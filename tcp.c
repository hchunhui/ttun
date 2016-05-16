#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <netinet/tcp.h>

int sock_create(char *ip, unsigned short port)
{
	int sockfd;
	int yes = 1;
	struct sockaddr_in addr;
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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
		return -1;
	}
	return sockfd;
}

void set_tcp_nodelay(int fd)
{
	int enable = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
}

void fwd(int ifd, int ofd)
{
	static char buf[4096];
	int ret;
	ret = read(ifd, buf, 4096);
	if(ret <= 0) {
		perror("fwd_read");
		exit(1);
	} else {
		int last = ret;
		int p = 0;
		while(last) {
			ret = write(ofd, buf + p, last);
			if(ret <= 0) {
				perror("fwd_write");
				exit(1);
			}
			last -= ret;
			p += ret;
		}
	}
}

int main(int argc, char **argv)
{
	int peer_fd;

	if(argc < 4)
		exit(1);
	if(argv[1][0] == 'c') {
		struct sockaddr_in peer_addr;
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
	} else {
		int listen_fd;
		listen_fd = sock_create(argv[2], atoi(argv[3]));
		listen(listen_fd, 10);
		peer_fd = accept(listen_fd, NULL, NULL);
		if(peer_fd < 0) {
			perror("accept");
			exit(1);
		}
		close(listen_fd);
	}

	set_tcp_nodelay(peer_fd);

	if(argc >= 5) {
			dup2(peer_fd, 0);
			dup2(peer_fd, 1);
			close(peer_fd);
			execvp(argv[4], argv + 4);
	} else {
		fd_set rfds;
		int ret;

		while (1) {
			FD_ZERO(&rfds);
			FD_SET(0, &rfds);
			FD_SET(peer_fd, &rfds);

			ret = select(peer_fd + 1, &rfds, NULL, NULL, NULL);
			if(ret == -1) {
				perror("select");
				exit(1);
			} else if(ret) {
				if(FD_ISSET(peer_fd, &rfds))
					fwd(peer_fd, 1);
				if(FD_ISSET(0, &rfds))
					fwd(0, peer_fd);
			}
		}
	}
	return 0;
}
