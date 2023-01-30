#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <glob.h>

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
  if (argc != 3) {
    printUsage();
    error("Invalid number of arguments.\n", -1);
  }

  int    sockfd, portno, n;
  size_t serverlen;
  struct sockaddr_in serveraddr;
  struct hostent *server;
  char *hostname;

  // TODO: More argument parsing for socket connection
  hostname = argv[1];
  portno   = atoi(argv[2]);

  /* socket: create the socket */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");

  /* gethostbyname: get the server's DNS entry */
  server = gethostbyname(hostname);
  if (server == NULL) {
    fprintf(stderr, "ERROR, no such host as %s\n", hostname);
    exit(0);
  }

  // Build the server address
  serverlen = sizeof(serveraddr);
  bzero((void *) &serveraddr, serverlen);
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(portno);
  bcopy((void *) &server->h_addr_list[0], (void *) &serveraddr.sin_addr, );

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
      if (arg2 == NULL) {
        puts("Must provide a path argument to 'get' command.");
        continue;
      }
      // TODO: Send command for get and recieve response
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
