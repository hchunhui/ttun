#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>

int sock_create(char *ip, unsigned short port)
{
	int sockfd;
	int yes = 1;
	struct sockaddr_in addr;
	int optval;
	socklen_t optlen = sizeof(optval);

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		perror("reuseaddr");
                exit(1);
	}

	optval = 1;
	optlen = sizeof(optval);
	if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
		perror("setsockopt()");
		exit(1);
	}

	optval = 3;
	optlen = sizeof(optval);
	if (setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, &optval, optlen) < 0) {
		perror("setsockopt()");
		exit(1);
	}

	optval = 5;
	optlen = sizeof(optval);
	if (setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, &optval, optlen) < 0) {
		perror("setsockopt()");
		exit(1);
	}

	optval = 550;
	optlen = sizeof(optval);
	if (setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, &optval, optlen) < 0) {
		perror("setsockopt()");
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

int xconnect(int fd, struct sockaddr *addr, int addrlen, int timeo)
{
	socklen_t len = sizeof(errno);
	int flags = fcntl(fd, F_GETFL, 0);
	struct timeval tm = { timeo, 0 };
	fd_set set;
	int ret;

	if (flags < 0) {
		perror("getfl");
		exit(1);
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		perror("setfl");
		exit(1);
	}

	ret = connect(fd, addr, addrlen);
	if (ret < 0) {
		FD_ZERO(&set);
		FD_SET(fd, &set);

		if (select(fd + 1, NULL, &set, NULL, &tm) > 0) {
			getsockopt(fd, SOL_SOCKET, SO_ERROR, &errno, &len);
			ret = errno ? -1 : 0;
		}
	}

	if (fcntl(fd, F_SETFL, flags) < 0) {
		perror("setfl2");
		exit(1);
	}

	return ret;
}

int fwd(int ifd, int ofd)
{
	static char buf[4096];
	int ret;
	ret = read(ifd, buf, 4096);
	if(ret < 0) {
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

	return ret;
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
		if(xconnect(peer_fd, (struct sockaddr *)&peer_addr, sizeof(peer_addr), 10)) {
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
		int flag1 = 1, flag2 = 1;

		while (flag1 || flag2) {
			FD_ZERO(&rfds);
			if(flag1)
				FD_SET(0, &rfds);
			if(flag2)
				FD_SET(peer_fd, &rfds);

			ret = select(peer_fd + 1, &rfds, NULL, NULL, NULL);
			if(ret == -1) {
				perror("select");
				exit(1);
			} else if(ret) {
				if(FD_ISSET(peer_fd, &rfds))
					if(fwd(peer_fd, 1) == 0) {
						flag1 = 0;
						flag2 = 0; /* XXX */
					}
				if(FD_ISSET(0, &rfds))
					if(fwd(0, peer_fd) == 0) {
						shutdown(peer_fd, SHUT_WR);
						flag1 = 0;
					}
			}
		}
	}
	return 0;
}
