CC?=$(CROSS_COMPILE)gcc
CFLAGS?=-g -Wall -Werror
LDFLAGS?=-lpthread -lrt

default: aesdsocket

all: clean aesdsocket

aesdsocket.o: aesdsocket.c 
	$(CC) -c aesdsocket.c  $(LDFLAGS) -o aesdsocket.o -DUSE_AESD_CHAR_DEVICE=1 $(CFLAGS)

aesdsocket: aesdsocket.o
	$(CC) aesdsocket.o $(LDFLAGS) -o aesdsocket -DUSE_AESD_CHAR_DEVICE=1 $(CFLAGS)

.PHONY: clean

clean:
	rm -f aesdsocket 
	rm -f aesdsocket.o