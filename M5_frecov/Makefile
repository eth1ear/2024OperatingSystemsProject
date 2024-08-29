CC=gcc
CFLAGS=-Wall -g

all: frecov

frecov: frecov.o
	$(CC) $(CFLAGS) -o frecov frecov.o

frecov.o: frecov.c
	$(CC) $(CFLAGS) -c frecov.c

clean:
	rm -f *.o frecov

