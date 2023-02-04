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

  int sockfd, n;
  // size_t             serverlen;
  struct sockaddr_in serveraddr;
  // struct hostent    *server;
  char *hostname;
  char *port;

  // TODO: More argument parsing for socket connection
  hostname = argv[1];
  portno   = argv[2];

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
  // Do this for the server
  // hints.ai_flags    = AI_PASSIVE;

  // Do DNS lookup with getaddrinfo()
  if (0 != (status = getaddrinfo(hostname, port, &hints, &servinfo)))
  {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(-3);
  }

  // Do something
  // struct addrinfo

  freeaddrinfo(servinfo);

    /* gethostbyname: get the server's DNS entry */
    // server = gethostbyname(hostname);
    // if (server == NULL) {
    //   fprintf(stderr, "ERROR, no such host as %s\n", hostname);
    //   exit(0);
    // }

    // printf("Hostname:Port %s:%d\n", server->h_addr, portno);

    // // Build the server address
    // serverlen = sizeof(serveraddr);
    // bzero((char *)&serveraddr, serverlen);
    // // Set family and port
    // serveraddr.sin_family = AF_INET;
    // serveraddr.sin_port   = htons(portno);
    // // Copy the address returned from DNS
    // bcopy((char *)&server->h_addr, (char *)&serveraddr.sin_addr.s_addr,
    //       (size_t)server->h_length);

    /* build the server's Internet address */
    // bzero((char *)&serveraddr, sizeof(serveraddr));
    // serveraddr.sin_family = AF_INET;
    // bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr,
    //       server->h_length);
    // serveraddr.sin_port = htons(portno);

    // Begin an indefinite command input loop
    char *cmd = NULL;
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
    // size_t s = strlen(cmd);
    // if (1 != sscanf(cmd, "%m[a-z]", &opcode)) {
    //   printf("Invalid command.\n");
    //   continue;
    // }
    // Update command pointer to read the second argument (a little jenky but
    // ok)
    // if (s > strlen(opcode)) {
    //   printv("TEST");
    //   cmd_argc = 2;
    //   cmd += strlen(opcode) + 1;
    //   // Remove the trailing newline for globbing
    //   s = strlen(cmd);
    //   if (s && (cmd[s - 1] == '\n'))
    //     cmd[--s] = 0;
    // }

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
      char *test = "hello";
      // n = sendto(sockfd, test, strlen(test), 0, &serveraddr, serverlen);
      // if (n < 0) {
      //   puts("Error in sendto.");
      //   // break;
      // }
      printf("N = %d\n", n);
      // break;

      /* print the server's reply */
      // n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);
      // if (n < 0)
      //   error("ERROR in recvfrom", -5);
      // printf("Echo from server: %s", buf);
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
      for (int i = 0; i < paths.gl_pathc; ++i) {
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
