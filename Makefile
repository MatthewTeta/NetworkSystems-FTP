CC=gcc
CFLAGS=-O3 -Wall -Wextra -I.

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

all: ftp_client ftp_server

ftp_client: src/ftp_client.o src/ftp_protocol.o src/util.h
	mkdir -p client && $(CC) -o client/$@ $^ $(CFLAGS)

ftp_server: src/ftp_server.o src/ftp_protocol.o src/util.h
	mkdir -p server && $(CC) -o server/$@ $^ $(CFLAGS)


.PHONY: clean

clean:
	rm -f **/*.o *~ client/ftp_client server/ftp_server
