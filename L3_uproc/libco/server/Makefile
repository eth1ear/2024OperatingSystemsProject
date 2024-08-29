.PHONY: test libco

all: libco server_ optServer_

server: libco all
	@echo "==== TEST 64 bit ===="
	@LD_LIBRARY_PATH=.. ./server

optServer: libco all
	@echo "==== TEST 64 bit(optServer) ===="
	@LD_LIBRARY_PATH=.. ./optServer

normalServer: normalServer.c
	@gcc normalServer.c -g -o normalServer
	@echo "==== TEST 64 bit(normalServer) ===="
	@./normalServer

libco:
	@cd .. && make -s

server_: server.c
	gcc -I.. -L.. -m64 server.c -g -o server -lco-64 -O0

optServer_: optimizeServer.c
	gcc -I.. -L.. -m64 optimizeServer.c -g -o optServer -lco-64 -O2

clean:
	rm -f server
