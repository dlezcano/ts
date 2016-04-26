CFLAGS?=-g -Wall -fPIC
CC=gcc
LDFLAGS=-ldl -lssl -lcrypto

SRC=$(wildcard *.c)
OBJS=$(SRC:%.c=%.o)
BIN=ts

SUBDIRS=plugins sensors

.PHONY:  $(SUBDIRS)

default: $(BIN) $(SUBDIRS)

%.o: %.c %h Makfile
	$(CROSS_COMPILE)$(CC) -c -o $@ $< $(CFLAGS)

$(BIN): $(OBJS)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

perm:
	setcap cap_sys_rawio,cap_sys_admin,cap_dac_override=ep $(BIN)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	rm -f $(OBJS) $(BIN)
