CC = cc
CFLAGS = -Wall
LDFLAGS = -pthread

all: rpc.o

rpc.o: rpc.c rpc.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f rpc.o

.PHONY: all clean