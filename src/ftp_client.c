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

  int                sockfd, portno;
  struct sockaddr_in serveraddr;
  char              *hostname;
  char              *port;

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

  ftp_set_socket(sockfd, (struct sockaddr *)(&serveraddr), sizeof(serveraddr));

  freeaddrinfo(servinfo);

  // Begin an indefinite command input loop
  char     *cmd = NULL;
  size_t    buflen;
  ssize_t   len;
  char     *opcode = NULL;
  char     *arg2   = NULL;
  ftp_err_t rv;
  FILE     *fp;

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

      // Initialize GET transaction (GET <filename>)
      // Wait for ACK or error
      // Begin data transmission
      
      if (!access(arg2, F_OK)) {
        perror("A file with the specified name already exists locally.");
        continue;
      }

      // Send GET request
      rv = ftp_send_chunk(FTP_CMD_GET, arg2, -1, 1);
      if (rv != FTP_ERR_NONE) {
        error("Error sending GET command", -3);
        continue;
      }

      // Recieve into a file
      fp = fopen(arg2, "w");
      rv = ftp_recv_data(fp, NULL, NULL);
      fclose(fp);
      if (rv != FTP_ERR_NONE && rv != FTP_ERR_SERVER) {
        error("Error recv GET command\n", -3);
      }
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
      rv = ftp_send_chunk(FTP_CMD_LS, NULL, 0, 1);
      if (rv != FTP_ERR_NONE) {
        fprintf(stderr, "Error in LS ftp_send_chunk\n");
        // if (rv == FTP_ERR_SERVER) {
        //   // Print the server error
        //   ftp_perror();
        // }
        continue;
      }

      // Recieve the response into the stdout
      rv = ftp_recv_data(stdout, NULL, NULL);
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
