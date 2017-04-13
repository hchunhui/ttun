CROSS ?=
CC = $(CROSS)gcc
STRIP = $(CROSS)strip
CFLAGS = -O2
BIN = $(CROSS)bin

.PHONY: all kcp clean

all: $(BIN) $(BIN)/pipe $(BIN)/tcp $(BIN)/ttun $(BIN)/inspect
kcp: $(BIN) $(BIN)/kcp

$(BIN):
	mkdir -p $(BIN)
	cp run_* $(BIN)
$(BIN)/ttun: ttun.c
$(BIN)/pipe: pipe.c
$(BIN)/tcp: tcp.c
$(BIN)/inspect: inspect.c
$(BIN)/kcp: kcp.c
	$(CC) $(CFLAGS) -o $@ $< ikcp/ikcp.c

$(BIN)/%: %.c
	$(CC) $(CFLAGS) -o $@ $<
	$(STRIP) $@

clean:
	rm -rf $(BIN)/pipe $(BIN)/tcp $(BIN)/ttun $(BIN)/inspect $(BIN)/kcp *~
