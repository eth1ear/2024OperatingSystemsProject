CC=gcc
CFLAGS=-Wall -g

all: pstree

pstree: pstree.o
	$(CC) $(CFLAGS) -o pstree pstree.o

pstree.o: pstree.c
	$(CC) $(CFLAGS) -c pstree.c

clean:
	rm -f *.o pstree

