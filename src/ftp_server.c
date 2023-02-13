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
// void printv(char *msg) {
// #ifdef VERBOSE
//     puts(msg);
// #endif
// }

/**
 * Wrapper around perror, exit with {code}
 */
void error(char *msg, int code) {
    fprintf(stderr, "%s\n", msg);
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
    FILE     *fp;

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

    ftp_set_socket(sockfd, NULL, 0);

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
        // Recieve command to initialize a transaction, manually send ack to
        // allow for error propogation
        rv = ftp_recv_chunk(&chunk, -1, 0, cliaddr, &clientlen);
        if (rv != FTP_ERR_NONE) {
            error("Error recieving chunk in server\n", -4);
        }

        ftp_set_socket(sockfd, cliaddr, clientlen);

        // printf("SERVER RECV: %02X\n", 0xFF & chunk.cmd);

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

        // Detertmine which command got sent to the server and handle
        // synchronously (single client)
        switch (chunk.cmd) {


        case FTP_CMD_GET:
            // printv("SERVER GET");
            // Check if the file exists
            if (access(chunk.packet, F_OK)) {
                ftp_send_chunk(FTP_CMD_ERROR,
                               "The specified file does not exists in the "
                               "server file system.\n",
                               -1, 0);
                continue;
            }
            // Send ACK
            ftp_send_chunk(FTP_CMD_ACK, NULL, 0, 0);
            // Open the file
            fp = fopen(chunk.packet, "r");
            ftp_send_data(fp);
            fclose(fp);
            break;


        case FTP_CMD_PUT:


            // printv("SERVER PUT");
            if (!access(chunk.packet, F_OK)) {
                ftp_send_chunk(FTP_CMD_ERROR,
                               "A file with the specified name already exists "
                               "on the server.\n",
                               -1, 0);
                continue;
            }

            // Send ACK
            ftp_send_chunk(FTP_CMD_ACK, NULL, 0, 0);

            // Recieve into a file
            fp = fopen(chunk.packet, "w");
            rv = ftp_recv_data(fp, NULL, NULL);
            fclose(fp);
            if (rv != FTP_ERR_NONE && rv != FTP_ERR_SERVER) {
                fprintf(stderr, "Error recv PUT command\n");
            }
            break;


        case FTP_CMD_DELETE:


            // printv("SERVER DELETE");
            if (access(chunk.packet, F_OK)) {
                ftp_send_chunk(
                    FTP_CMD_ERROR,
                    "The specified file does not exists on the server\n", -1,
                    0);
                continue;
            }
            ftp_send_chunk(FTP_CMD_ACK, NULL, 0, 0);
            // construct the rm command using the specified file name
            char *rm = "rm -vf ";
            char cmd[sizeof(rm) + FTP_PACKETSIZE];
            snprintf(cmd, sizeof(rm) + FTP_PACKETSIZE, "%s%s", rm, chunk.packet);
            fp = popen(cmd, "r");
            if (!fp) {
                error("Failed to run the 'rm' command. Is it in your path?\n",
                      -2);
            }
            ftp_send_data(fp);
            if (-1 == pclose(fp))
                error("PCLOSE ERROR\n", -2);
            break;


        case FTP_CMD_LS:;


            // printv("SERVER LS");
            // SEND ACK
            ftp_send_chunk(FTP_CMD_ACK, NULL, 0, 0);
            // Execute the ls command and send the stdout over the socket
            fp = popen("/bin/ls -l", "r");
            if (!fp)
                error("Failed to run the 'ls' command. Is it in your path?\n",
                      -2);
            ftp_send_data(fp);
            if (-1 == pclose(fp))
                error("PCLOSE ERROR\n", -2);
            break;


        default:
            fprintf(stderr, "Invalid Command: 0x%02X\n", 0xFF & chunk.cmd);
            ftp_send_chunk(FTP_CMD_ERROR, "Invalid Command (Bad Programmer)\n",
                           -1, 0);
            break;
        }
    }
}
