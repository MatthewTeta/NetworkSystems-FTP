CC=gcc
CFLAGS=-I. -g -D VERBOSE

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

ftp_client: src/ftp_client.o
	$(CC) -o $@ $^ $(CFLAGS)

ftp_server: src/ftp_server.o
	$(CC) -o $@ $^ $(CFLAGS)


.PHONY: clean

clean:
	rm -f **/*.o *~ core
