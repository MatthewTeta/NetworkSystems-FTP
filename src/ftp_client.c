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

#define BUFSZ 1024

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
  char buf[BUFSZ];
  if (argc != 3) {
    printUsage();
    error("Invalid number of arguments.\n", -1);
  }

  int                sockfd, n, portno;
  socklen_t          serverlen;
  struct sockaddr_in serveraddr;
  // struct hostent    *server;
  char *hostname;
  char *port;

  // TODO: More argument parsing for socket connection
  hostname = argv[1];
  port     = argv[2];
  portno   = atoi(port);
  if (portno == 0) {
    error("Invalid port given as argument", -1);
  }

  /* socket: create the socket */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket", -3);

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

  // Do something
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
  size_t  buflen, s;
  ssize_t len;
  //   wordexp_t arglist;
  char  *opcode = NULL;
  char  *arg2   = NULL;
  glob_t paths;
  char   path[1024];

  // Loop forever accepting commands typed into stdin
  for (;;) {
    printf("> ");
    if ((len = getline(&cmd, &buflen, stdin)) <= 0)
      continue;

    // Parse command
    processCmdString(cmd, &opcode, &arg2);

    // Parse based on opcode
    if (0 == strcmp("get", opcode)) {
      printv("GET");
      printf("%s\n", cmd);
      if (arg2 == NULL) {
        puts("Must provide a path argument to 'get' command.");
        continue;
      }
      // TODO: Send command for get and recieve response
      n = sendto(sockfd, arg2, strlen(arg2), 0,
                 (struct sockaddr *)(&serveraddr), sizeof(serveraddr));
      if (n < 0) {
        puts("Error in sendto.");
        // break;
      }
      printf("N = %d\n", n);
      // break;

      /* print the server's reply */
      serverlen = sizeof(serveraddr);
      // We are assuming only one server, so we don't really need to check the
      // information stored into the serveraddr
      while (0 < (n = recvfrom(sockfd, buf, strlen(buf), 0,
                               (struct sockaddr *)(&serveraddr), &serverlen))) {
        printf("Echo from server: %s", buf);
        bzero(buf, strlen(buf));
      }
      if (n < 0)
        error("ERROR in recvfrom", -5);
    } else if (0 == strcmp("put", opcode)) {
      printv("PUT");
      if (arg2 == NULL) {
        puts("Must provide a path argument to 'put' command.");
        continue;
      }
      // TODO: Send file data to server in datagram chunks
      if (0 != glob(arg2, GLOB_MARK, NULL, &paths)) {
        printf("Error matching filepath.\n");
        continue;
      }
      // Iterate through the paths returned from glob search
      for (unsigned int i = 0; i < paths.gl_pathc; ++i) {
        // Ignore if this path is a dir
        s = strlen(paths.gl_pathv[i]);
        if (paths.gl_pathv[i][s - 1] == '/')
          continue;
        printf("\t[%d]: %s\n", i, paths.gl_pathv[i]);
      }
      globfree(&paths);
    } else if (0 == strcmp("delete", opcode)) {
      printv("DELETE");
      if (arg2 == NULL) {
        puts("Must provide a path argument to 'delete' command.");
        continue;
      }
    } else if (0 == strcmp("ls", opcode)) {
      printv("LS");
      // TODO: Move this to the server code
      FILE *fp = popen("/bin/ls", "r");
      if (!fp)
        error("Failed to run the ls command. Is it in your path?", -2);
      while (fgets(path, sizeof(path), fp) != NULL) {
        printf("%s", path);
      }
      // printf("%s", )
      if (-1 == pclose(fp))
        error("PCLOSE ERROR", -2);
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
