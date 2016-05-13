all: pipe tcp ttun
ttun: ttun.c
pipe: pipe.c
tcp: tcp.c
clean:
	rm -rf pipe tcp ttun *~
