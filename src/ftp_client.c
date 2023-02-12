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

// #define BUFSZ 1024

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

/**
 * convert a stdin string and split on the first space,
 * also replace the final newline character with a \0
 * for use with globbing
 */
int processCmdString(char *cmd, char **opcode, char **arg2) {
  if (!cmd || !opcode || !arg2)
    return -1;
  *opcode = cmd;
  *arg2   = NULL;
  while (*cmd != '\0') {
    if (!*arg2 && *cmd == ' ') {
      *cmd  = '\0';
      *arg2 = cmd + 1;
    }
    if (*cmd == '\n')
      *cmd = '\0';
    cmd++;
  }
  return 0;
}

void printUsage() {
  puts("Usage:\n\t./tcp_client <address> <port>");
  puts("\tInput commands take the following format:");
  puts("\t\tget\t<file_name>");
  puts("\t\tput\t<file_name>");
  puts("\t\tdelete\t<file_name>");
  puts("\t\tls");
  puts("\t\texit");
}

int main(int argc, char **argv) {
  // char buf[BUFSZ];
  if (argc != 3) {
    printUsage();
    error("Invalid number of arguments.\n", -1);
  }

  int sockfd, portno;
  // socklen_t          serverlen;
  struct sockaddr_in serveraddr;
  // struct hostent    *server;
  char *hostname;
  char *port;

  hostname = argv[1];
  port     = argv[2];
  portno   = atoi(port);
  if (portno == 0) {
    error("Invalid port given as argument\n", -1);
  }

  /* socket: create the socket */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket\n", -3);

  int              status;
  struct addrinfo  hints;
  struct addrinfo *servinfo;

  bzero(&hints, sizeof(hints));
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  // Do this for the server
  // hints.ai_flags    = AI_PASSIVE;

  // Do DNS lookup with getaddrinfo()
  if (0 != (status = getaddrinfo(hostname, port, &hints, &servinfo))) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(-3);
  }

  // Check that there is at least 1 result
  if (servinfo == NULL) {
    fprintf(stderr, "No address found for hostname: %s\n", hostname);
    exit(-4);
  }

  // Convert packed address to presentation format and print
  char                ipstr[INET_ADDRSTRLEN];
  struct sockaddr_in *sockaddr = (struct sockaddr_in *)servinfo->ai_addr;
  inet_ntop(servinfo->ai_family, &(sockaddr->sin_addr), ipstr, sizeof(ipstr));
  printf("IP From DNS: %s\n", ipstr);

  // Populate struct sockaddr_in serveraddr; used for sendto
  bzero(&serveraddr, sizeof(struct sockaddr_in));
  serveraddr.sin_family = servinfo->ai_family; // Should be AF_INET
  memcpy(&serveraddr.sin_addr, &sockaddr->sin_addr,
         sizeof(struct in_addr)); // Result from DNS lookup
  serveraddr.sin_port = htons(portno);

  freeaddrinfo(servinfo);

  // Begin an indefinite command input loop
  char   *cmd = NULL;
  size_t  buflen;
  ssize_t len;
  //   wordexp_t arglist;
  char *opcode = NULL;
  char *arg2   = NULL;
  // glob_t paths;
  // char      path[1024];
  ftp_err_t rv;

  // Loop forever accepting commands typed into stdin
  for (;;) {
    printf("> ");
    if ((len = getline(&cmd, &buflen, stdin)) <= 0)
      continue;

    // Parse command
    processCmdString(cmd, &opcode, &arg2);

    if (arg2 != NULL && strlen(arg2) > FTP_PACKETSIZE) {
      perror("Filename is too long for current implementation!");
      continue;
    }

    // Parse based on opcode
    if (0 == strcmp("get", opcode)) {
      printv("GET");
      if (arg2 == NULL) {
        puts("Must provide a path argument to 'get' command.");
        continue;
      }
      // TODO: Send command for get and recieve response
      // uint8_t buf[1024];
      // for (int i = 0; i < 1024; ++i)
      //   buf[i] = (i / 4) & 0xFF;

      // ftp_err_t rv =
      // ftp_send_data(sockfd, buf, 1024, (struct sockaddr *)(&serveraddr),
      //               sizeof(serveraddr));

      // Send GET request
      rv = ftp_send_chunk(sockfd, FTP_CMD_GET, arg2, strlen(arg2),
                          (struct sockaddr *)(&serveraddr), sizeof(serveraddr));
      if (rv != FTP_ERR_NONE) {
        error("Error sending GET command", -3);
      }

      // TODO: Get ACK
      // ftp_cmd_t cmd;
      // rv = ftp_recv_chunk(sockfd, &cmd, )

      // Recieve into stdout
      rv = ftp_recv_data(sockfd, stdout, NULL, NULL);
      if (rv != FTP_ERR_NONE) {
        error("Error recv GET command\n", -3);
      }

      // n = sendto(sockfd, arg2, strlen(arg2), 0,
      //            (struct sockaddr *)(&serveraddr), sizeof(serveraddr));
      // if (n < 0) {
      //   puts("Error in sendto.");
      //   // break;
      // }
      // printf("N = %d\n", n);
      // // break;

      // /* print the server's reply */
      // serverlen = sizeof(serveraddr);
      // printf("serverlen: %u\n", serverlen);
      // // We are assuming only one server, so we don't really need to check
      // the
      // // information stored into the serveraddr
      // while (1) {
      //   puts("LOOP");
      //   // Poll to allow for a timeout
      //   short         revents = 0;
      //   struct pollfd fds     = {
      //           .fd      = sockfd,
      //           .events  = POLLIN,
      //           .revents = revents,
      //   };
      //   int rv = poll(&fds, 1, FTP_TIMEOUT_MS);
      //   if (rv < 0) {
      //     error("Polling error", -6);
      //   } else if (rv == 0) {
      //     puts("Timeout occured from poll");
      //     break;
      //   } else {
      //     // Event happened
      //     puts("Event happened");
      //     if (revents & (POLLERR | POLLNVAL)) {
      //       error("There was an error with the file descriptor when polling",
      //             -6);
      //     }
      //     // There is data to be read from the pipe
      //     bzero(buf, BUFSZ);
      //     int nrec = recvfrom(sockfd, buf, BUFSZ, 0,
      //                         (struct sockaddr *)(&serveraddr), &serverlen);
      //     printf("nrec: %d\n", nrec);
      //     if (nrec < 0)
      //       error("ERROR in recvfrom", -5);
      //     printf("serverlen: %u\n", serverlen);
      //     printf("Echo from server: %s", buf);
      //     bzero(buf, strlen(buf));
      //     if (nrec == n)
      //       break;
      //   }
      // }
    } else if (0 == strcmp("put", opcode)) {
      printv("PUT");
      if (arg2 == NULL) {
        puts("Must provide a path argument to 'put' command.");
        continue;
      }
      printf("PUT %s\n", arg2);
      // // TODO: Send file data to server in datagram chunks
      // if (0 != glob(arg2, GLOB_MARK, NULL, &paths)) {
      //   printf("Error matching filepath.\n");
      //   continue;
      // }
      // // Iterate through the paths returned from glob search
      // for (unsigned int i = 0; i < paths.gl_pathc; ++i) {
      //   // Ignore if this path is a dir
      //   s = strlen(paths.gl_pathv[i]);
      //   if (paths.gl_pathv[i][s - 1] == '/')
      //     continue;
      //   printf("\t[%d]: %s\n", i, paths.gl_pathv[i]);
      // }
      // globfree(&paths);
    } else if (0 == strcmp("delete", opcode)) {
      printv("DELETE");
      if (arg2 == NULL) {
        puts("Must provide a path argument to 'delete' command.");
        continue;
      }
    } else if (0 == strcmp("ls", opcode)) {
      printv("LS");
      rv = ftp_send_chunk(sockfd, FTP_CMD_LS, NULL, 0,
                          (struct sockaddr *)(&serveraddr), sizeof(serveraddr));
      if (rv != FTP_ERR_NONE) {
        fprintf(stderr, "Error in LS ftp_send_chunk\n");
        // if (rv == FTP_ERR_SERVER) {
        //   // Print the server error
        //   ftp_perror();
        // }
        continue;
      }

      // Recieve the response into the stdout
      rv = ftp_recv_data(sockfd, stdout, NULL, NULL);
      if (rv != FTP_ERR_NONE) {
        fprintf(stderr, "Error in LS ftp_recv_data\n");
        // if (rv == FTP_ERR_SERVER)
        //   ftp_perror();
        continue;
      }
    } else if (0 == strcmp("exit", opcode)) {
      printv("EXIT");
      exit(0);
    } else {
      printf("Invalid command.\n");
      continue;
    }
  }

  return 0;
}
