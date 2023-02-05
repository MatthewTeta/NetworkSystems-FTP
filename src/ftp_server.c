#include <arpa/inet.h>
#include <glob.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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
  int                sockfd;       /* socket */
  int                portno;       /* port to listen on */
  socklen_t          clientlen;    /* byte size of client's address */
  struct sockaddr_in serveraddr;   /* server's addr */
  struct sockaddr_in clientaddr;   /* client addr */
  struct hostent    *hostp;        /* client host info */
  char               buf[BUFSIZE]; /* message buf */
  char              *hostaddrp;    /* dotted decimal host addr string */
  int                optval;       /* flag value for setsockopt */
  int                n;            /* message byte size */

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
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr,
                 &clientlen);
    if (n < 0)
      error("ERROR in recvfrom", -3);

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
    printf("server received datagram from %s (%s)\n", hostp->h_name, hostaddrp);
    printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);

    /*
     * sendto: echo the input back to the client
     */
    // while (1)
    // n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&clientaddr,
    //            clientlen);
    if (n < 0)
      error("ERROR in sendto", -3);
  }
}
