#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "ftp_protocol.h"

#define BUFSIZE 1024

/**
 * Print a message if VERBOSE is defined
 */
void printv(char *msg) {
#ifdef VERBOSE
  puts(msg);
#endif
}

/**
 * Wrapper around perror, exit with {code}
 */
void error(char *msg, int code) {
  perror(msg);
  exit(code);
}

int main(int argc, char **argv) {
  int                sockfd;     /* socket */
  int                portno;     /* port to listen on */
  socklen_t          clientlen;  /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent    *hostp;      /* client host info */
  // char               buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int   optval;    /* flag value for setsockopt */
  // int       n;         /* message byte size */
  ftp_err_t rv;

  /*
   * check command line arguments
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /*
   * socket: create the parent socket
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket", -3);

  /* setsockopt: Handy debugging trick that lets
   * us rerun the server immediately after we kill it;
   * otherwise we have to wait about 20 secs.
   * Eliminates "ERROR on binding: Address already in use" error.
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
             sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *)&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family      = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port        = htons((unsigned short)portno);

  /*
   * bind: associate the parent socket with a port
   */
  if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    error("ERROR on binding", -3);

  /*
   * main loop: wait for a datagram, then echo it
   */
  struct sockaddr *cliaddr = (struct sockaddr *)&clientaddr;
  clientlen                = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    // bzero(buf, BUFSIZE);
    ftp_chunk_t chunk;
    rv = ftp_recv_chunk(sockfd, &chunk, -1, cliaddr, &clientlen);
    if (rv != FTP_ERR_NONE) {
      error("Error recieving chunk in server\n", -4);
    }

    printf("SERVER RECV: %02X\n", 0xFF & chunk.cmd);

    /*
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
                          sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr", -3);
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n", -3);
    // printf("server received datagram from %s (%s)\n", hostp->h_name,
    // hostaddrp); printf("server received %u/%d bytes\n", chunk.nbytes,
    // FTP_PACKETSIZE);

    // Detertmine which command got sent to the server and handle synchronously
    // (single client)
    switch (chunk.cmd) {
    case FTP_CMD_GET:
      break;
    case FTP_CMD_PUT:
      break;
    case FTP_CMD_DELETE:
      break;
    case FTP_CMD_LS:;
      printf("SERVER LS\n");
      // Execute the ls command and send the stdout over the socket
      char  path[1024];
      FILE *fp = popen("/bin/ls", "r");
      if (!fp)
        error("Failed to run the 'ls' command. Is it in your path?\n", -2);
      while (fgets(path, sizeof(path), fp) != NULL) {
        ftp_send_data(sockfd, (uint8_t *)path, strlen(path), cliaddr,
                      clientlen);
      }

      // Send TERM
      ftp_send_chunk(sockfd, FTP_CMD_TERM, NULL, 0, cliaddr, clientlen);

      // printf("%s", )
      if (-1 == pclose(fp))
        error("PCLOSE ERROR\n", -2);

      break;
    default:
      fprintf(stderr, "Invalid Command: 0x%02X\n", 0xFF & chunk.cmd);
      ftp_send_chunk(sockfd, FTP_CMD_ERROR,
                     (uint8_t *)"Invalid Command (Bad Programmer)",
                     FTP_PACKETSIZE, cliaddr, clientlen);
      break;
    }

  }
}
