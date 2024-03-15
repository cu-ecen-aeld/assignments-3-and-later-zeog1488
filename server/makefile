CC?=$(CROSS_COMPILE)gcc
CFLAGS?=-g -Wall -Werror
CFLAGS+=-DUSE_AESD_CHAR_DEVICE=1
LDFLAGS?=-lpthread -lrt

default: aesdsocket

all: clean aesdsocket

aesdsocket.o: aesdsocket.c 
	$(CC) -c aesdsocket.c  $(LDFLAGS) -o aesdsocket.o $(CFLAGS)

aesdsocket: aesdsocket.o
	$(CC) aesdsocket.o $(LDFLAGS) -o aesdsocket $(CFLAGS)

.PHONY: clean

clean:
	rm -f aesdsocket 
	rm -f aesdsocket.o