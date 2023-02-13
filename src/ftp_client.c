#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "ftp_protocol.h"

// #define BUFSZ 1024

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

void printUsage(char *exe_name) {
    printf("Usage:\n\t%s <address> <port>\n", exe_name);
    puts("\tInput commands take the following format:");
    puts("\t\tget\t<file_name>");
    puts("\t\tput\t<file_name>");
    puts("\t\tdelete\t<file_name>");
    puts("\t\tls");
    puts("\t\texit");
}

void sigint_handler(int sig) {
    if (sig == SIGINT) {
        // Cancel any ongoing ftp transaction
        ftp_exit();
    }
    exit(-1);
}

int main(int argc, char **argv) {
    // char buf[BUFSZ];
    if (argc != 3) {
        printUsage(argv[0]);
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

    // Set up the signal handler for Ctrl-C
    signal(SIGINT, sigint_handler);

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

    ftp_set_socket(sockfd, (struct sockaddr *)(&serveraddr),
                   sizeof(serveraddr));

    freeaddrinfo(servinfo);

    // Begin an indefinite command input loop
    char     *cmd = NULL;
    size_t    buflen;
    ssize_t   len;
    char     *opcode = NULL;
    char     *arg2   = NULL;
    ftp_err_t rv;
    FILE     *fp;
    clock_t   start, end;
    double    cpu_time_used;

    // Loop forever accepting commands typed into stdin
    for (;;) {
        printf("> ");
        if ((len = getline(&cmd, &buflen, stdin)) <= 0)
            continue;

        // Parse command
        processCmdString(cmd, &opcode, &arg2);

        if (arg2 != NULL && strlen(arg2) > FTP_PACKETSIZE) {
            fprintf(stderr,
                    "Filename is too long for current implementation!\n");
            continue;
        }

        // Parse based on opcode
        if (0 == strcmp("get", opcode)) {
            // printv("GET");
            if (arg2 == NULL) {
                puts("Must provide a path argument to 'get' command.");
                continue;
            }

            if (!access(arg2, F_OK)) {
                fprintf(
                    stderr,
                    "A file with the specified name already exists locally.\n");
                continue;
            }

            start = clock();
            // Send GET request
            rv = ftp_send_chunk(FTP_CMD_GET, arg2, -1, 1);
            if (rv != FTP_ERR_NONE) {
                fprintf(stderr, "Error sending GET command\n");
                continue;
            }

            // Recieve into a file
            fp = fopen(arg2, "w");
            rv = ftp_recv_data(fp, NULL, NULL);
            fclose(fp);
            if (rv != FTP_ERR_NONE && rv != FTP_ERR_SERVER) {
                fprintf(stderr, "Server: Error recv GET command\n");
                continue;
            }

            end           = clock();
            cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
            printf("GET completed successfully in %lfs\n", cpu_time_used);

        } else if (0 == strcmp("put", opcode)) {

            // printv("PUT");
            if (arg2 == NULL) {
                puts("Must provide a path argument to 'put' command.");
                continue;
            }
            // Check if the file exists
            if (access(arg2, F_OK)) {
                fprintf(stderr, "The specified file does not exists in the "
                                "local file system.\n");
                continue;
            }

            start = clock();
            // Send PUT request
            rv = ftp_send_chunk(FTP_CMD_PUT, arg2, -1, 1);
            if (rv != FTP_ERR_NONE) {
                if (rv == FTP_ERR_TIMEOUT) {
                    fprintf(stderr,
                            "Server did not respond to PUT request in time.\n");
                } else {
                    fprintf(stderr, "Weird error in PUT\n");
                }
                continue;
            }
            // Open the file
            fp = fopen(arg2, "r");
            ftp_send_data(fp);
            fclose(fp);

            end           = clock();
            cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
            printf("GET completed successfully in %lfs\n", cpu_time_used);

        } else if (0 == strcmp("delete", opcode)) {

            // printv("DELETE");
            if (arg2 == NULL) {
                puts("Must provide a path argument to 'delete' command.");
                continue;
            }
            rv = ftp_send_chunk(FTP_CMD_DELETE, arg2, -1, 1);
            if (rv != FTP_ERR_NONE) {
                fprintf(stderr, "Error in DELETE ftp_send_chunk\n");
                continue;
            }

            // Recieve the response into the stdout
            rv = ftp_recv_data(stdout, NULL, NULL);
            if (rv != FTP_ERR_NONE) {
                fprintf(stderr, "Error in DELETE ftp_recv_data\n");
                // if (rv == FTP_ERR_SERVER)
                //   ftp_perror();
                continue;
            }

        } else if (0 == strcmp("ls", opcode)) {
            // printv("LS");
            rv = ftp_send_chunk(FTP_CMD_LS, NULL, 0, 1);
            if (rv != FTP_ERR_NONE) {
                fprintf(stderr, "Error in LS ftp_send_chunk\n");
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
            // printv("EXIT");
            exit(0);
        } else {
            printf("Invalid command.\n");
            continue;
        }
    }

    return 0;
}
