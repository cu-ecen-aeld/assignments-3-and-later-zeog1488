CC?=$(CROSS_COMPILE)gcc
CFLAGS?=-g -Wall -Werror
LDFLAGS?=-pthread

default: aesdsocket

all: aesdsocket

aesdsocket.o: aesdsocket.c 
	$(CC) $(LDFLAGS) -c aesdsocket.c -o aesdsocket.o $(CFLAGS)

aesdsocket: aesdsocket.o
	$(CC) $(LDFLAGS) aesdsocket.o -o aesdsocket $(CFLAGS)

.PHONY: clean

clean:
	rm -f aesdsocket 
	rm -f aesdsocket.o