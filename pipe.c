/* modified twinpipe-1.0.6 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

/*
 * No warranty. You are free to modify this source and to
 * distribute the modified sources, as long as you keep the
 * existing copyright messages intact and as long as you
 * remember to add your own copyright markings.
 * You are not allowed to distribute the program or modified versions
 * of the program without including the source code (or a reference to
 * the publicly available source) and this notice with it.
 */

/* Copyright (C) 1992,2000 Bisqwit (http://iki.fi/bisqwit/) */

static int pipe_1[2], pipe_2[2];
static void sulje(void)
{
	close(pipe_2[0]); close(pipe_2[1]);
	close(pipe_1[0]); close(pipe_1[1]);
}

int main(int argc, const char *const *argv)
{
	int pid;
	const char *fail="";

	if(argc != 3)
	{
		fprintf(stderr,
			"Usage: twinpipe 'command1' 'command2'\n"
			"The commands will be able to read each others output.\n");
		return -1;
	}

	if((pipe(pipe_1) < 0 && (fail = "pipe_1"))
	|| (pipe(pipe_2) < 0 && (fail = "pipe_2")))
	{
collapsed:
		fprintf(stderr, "Ouch, the world just collapsed (%s).\n", fail);
		perror(*argv);
		return -1;
	}

	if((pid = fork()) < 0)goto collapsed;
	if(pid)
	{
		dup2(pipe_1[0], 0);
		dup2(pipe_2[1], 1);
		sulje();
		execl("/bin/sh", "sh", "-c", argv[1], NULL);
		_exit(0);
	}
	else
	{
		dup2(pipe_2[0], 0);
		dup2(pipe_1[1], 1);
		sulje();
		execl("/bin/sh", "sh", "-c", argv[2], NULL);
		_exit(0);
	}
	return 0;
}
