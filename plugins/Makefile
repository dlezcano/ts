CFLAGS?=-g -Wall
CC=gcc

SRC=$(wildcard *.c)
PLUGINS=$(SRC:%.c=%.so)

default: $(PLUGINS)

%.so: %.c ../trace.o common.h
	$(CROSS_COMPILE)$(CC) -fPIC -shared -rdynamic -o $@ $< $(CFLAGS) ../trace.o

clean:
	rm -f $(PLUGINS)
