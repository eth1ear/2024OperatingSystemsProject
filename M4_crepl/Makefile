CC=gcc
CFLAGS=-Wall -g

all: crepl

crepl: crepl.o
	$(CC) $(CFLAGS) -o crepl crepl.o

crepl.o: crepl.c
	$(CC) $(CFLAGS) -c crepl.c

clean:
	rm -f *.o crepl

