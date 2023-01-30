CC=gcc
CFLAGS=-I. -D VERBOSE
OBJ = src/ftp_client.o 

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

ftp_client: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)


.PHONY: clean

clean:
	rm -f **/*.o *~ core
