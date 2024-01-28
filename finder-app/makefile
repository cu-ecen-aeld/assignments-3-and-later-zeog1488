ifdef CROSS_COMPILE
	ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
		CC=aarch64-none-linux-gnu-gcc
	else
		CC=gcc
	endif
else
	CC=gcc
endif

default: writer

writer.o: writer.c 
	$(CC) -c writer.c -o writer.o

writer: writer.o
	$(CC) writer.o -o writer

.PHONY: clean

clean:
	rm -f writer 
	rm -f writer.o