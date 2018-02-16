CC=gcc
CFLAGS= -g -pedantic-errors -Wall -std=c99

default: all	
	$(CC) -o netrecon netrecon.o

all:		netrecon.o netrecon.o

netrecon.o:	netrecon.c
	$(CC) $(CFLAGS) -c netrecon.c

.PHONY: clean

clean:
	rm -rf netrecon netrecon.o