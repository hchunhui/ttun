#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>

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

void fwd(int ifd, int ofd)
{
	static unsigned char buf[4096];
	int i, n;

	readn(ifd, buf, 4);
	n = (buf[2] << 8) | buf[3];
	readn(ifd, buf + 4, n - 4);
	fprintf(stderr, "packet %d -> %d, length %d:",
		ifd, ofd, n - 4);
	for(i = 0; i < n - 4; i++) {
		if(i % 16 == 0)
			fprintf(stderr, "\n");
		fprintf(stderr, "%02x ", buf[i + 4]);
	}
	fprintf(stderr, "\n\n");
	writen(ofd, buf, n);
}

int main(int argc, char **argv)
{
	if(argc > 2)
		exit(1);
	if(argc == 2) {
		int fd = open(argv[1], O_WRONLY);
		dup2(fd, 2);
		close(fd);
	}

	while (1)
		fwd(0, 1);

	return 0;
}
