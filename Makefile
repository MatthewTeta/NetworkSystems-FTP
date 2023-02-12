CC=gcc
CFLAGS=-Og -Wall -Wextra -I. -g -D VERBOSE

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

all: ftp_client ftp_server

ftp_client: src/ftp_client.o src/ftp_protocol.o src/util.h
	$(CC) -o client/$@ $^ $(CFLAGS)

ftp_server: src/ftp_server.o src/ftp_protocol.o src/util.h
	$(CC) -o server/$@ $^ $(CFLAGS)


.PHONY: clean

clean:
	rm -f **/*.o *~ ftp_client ftp_server
