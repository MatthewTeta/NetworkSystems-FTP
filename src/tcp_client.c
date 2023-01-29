#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
// #include <wordexp.h>
#include <glob.h>

#include "defines.h"

/**
 * Print a message if VERBOSE is defined
 */
void printv(char *msg) {
#ifdef VERBOSE
  puts(msg);
#endif
}

/**
 *   @brief Wrapper around perror, exit with {code}
 */
void error(char *msg, int code) {
  perror(msg);
  exit(code);
}

void printUsage() {
  printf("Usage:\n\t./tcp_client address port\n");
  printf("\tInput commands take the following format:\n");
  printf("\t\tget\t[file_name]\n");
  printf("\t\tput\t[file_name]\n");
  printf("\t\tdelete\t[file_name]\n");
  printf("\t\tls\n");
  printf("\t\texit\n");
}

int main(int argc, char **argv) {
  if (argc != 3) {
    printUsage();
    error("Invalid number of arguments.\n", -1);
  }

  // TODO: More argument parsing for socket connection

  // Begin an indefinite command input loop
  char   *cmd = NULL;
  size_t  buflen;
  ssize_t len;
  //   wordexp_t arglist;
  char  *opcode = NULL;
  glob_t paths;
  for (;;) {
    int cmd_argc = 1;
    printf("> ");
    if ((len = getline(&cmd, &buflen, stdin)) <= 0)
      continue;
    // Parse command
    size_t s = strlen(cmd);
    if (1 != sscanf(cmd, "%m[a-z]", &opcode)) {
      printf("Invalid command.\n");
      continue;
    }
    // Update command pointer to read the second argument (a little jenky but
    // ok)
    if (s > strlen(opcode)) {
      printv("TEST");
      cmd_argc = 2;
      cmd += strlen(opcode) + 1;
      // Remove the trailing newline for globbing
      s = strlen(cmd);
      if (s && (cmd[s - 1] == '\n'))
        cmd[--s] = 0;
    }

    // Parse based on opcode
    if (0 == strcmp("get", opcode)) {
      printv("GET");
      if (cmd_argc != 2) {
        puts("Must provide a path argument to 'get' command.");
        continue;
      }
      if (0 != glob(cmd, GLOB_MARK, NULL, &paths)) {
        printf("Error matching filepath.\n");
        continue;
      }
      for (int i = 0; i < paths.gl_pathc; ++i) {
        printf("\t[%d]: %s\n", i, paths.gl_pathv[i]);
        // paths.
      }
    } else if (0 == strcmp("put", opcode)) {
      printv("PUT");
      if (cmd_argc != 2) {
        puts("Must provide a path argument to 'put' command.");
        continue;
      }
    } else if (0 == strcmp("delete", opcode)) {
      printv("DELETE");
      if (cmd_argc != 2) {
        puts("Must provide a path argument to 'delete' command.");
        continue;
      }
    } else if (0 == strcmp("ls", opcode)) {
      printv("LS");
    } else if (0 == strcmp("exit", opcode)) {
      printv("EXIT");
      exit(0);
    } else {
      printf("Invalid command.\n");
      continue;
    }

    globfree(&paths);
    free(opcode);
  }

  return 0;
}
