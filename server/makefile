CC=$(CROSS_COMPILE)gcc

default: aesdsocket

all: aesdsocket

aesdsocket.o: aesdsocket.c 
	$(CC) -pthread -c aesdsocket.c -o aesdsocket.o -Wall -Werror -g

aesdsocket: aesdsocket.o
	$(CC) -pthread aesdsocket.o -o aesdsocket -Wall -Werror -g

.PHONY: clean

clean:
	rm -f aesdsocket 
	rm -f aesdsocket.o