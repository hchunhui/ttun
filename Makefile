CROSS ?=
CC = $(CROSS)gcc
STRIP = $(CROSS)strip
CFLAGS = -O2
BIN = $(CROSS)bin

all: $(BIN) $(BIN)/pipe $(BIN)/tcp $(BIN)/ttun

$(BIN):
	mkdir -p $(BIN)
	cp run_* $(BIN)
$(BIN)/ttun: ttun.c
$(BIN)/pipe: pipe.c
$(BIN)/tcp: tcp.c

$(BIN)/%: %.c
	$(CC) $(CFLAGS) -o $@ $<
	$(STRIP) $@

clean:
	rm -rf $(BIN)/pipe $(BIN)/tcp $(BIN)/ttun *~
