#ifndef FTP_PROTOCOL
#define FTP_PROTOCOL

/**
 * Define the communication protocol shared between the client and the server.
 * This includes the valid command enums, packet size and format,
 * and methods for sending and recieving the data
 */

#include <stdint.h>
#include <sys/socket.h>

#define FTP_PACKETSIZE 512
#define FTP_TIMEOUT_MS 3000

/**
 * Client oriented command naming convention
 * Commands:
 *      GET <filename>: move <filename> file from server to client.
 *      PUT <filename>: move <filename> file from cleint to server.
 *      DELETE <filename>: delete <filename> file from server fs.
 *      LS : list the contents of the server filesystem.
 *      // Internal flow commands
 *      DATA : Any packet of bytes for a file
 *      ERROR <message>: Stop any ongoing partial transaction.
 *      TERM : Indiciate that the transaction in finished. Sentinal
 */
#define FTP_CMD_GET    ((uint8_t)0x01)
#define FTP_CMD_PUT    ((uint8_t)0x02)
#define FTP_CMD_DELETE ((uint8_t)0x03)
#define FTP_CMD_LS     ((uint8_t)0x04)
#define FTP_CMD_DATA   ((uint8_t)0x05)
#define FTP_CMD_ERROR  ((uint8_t)0x06)
#define FTP_CMD_TERM   ((uint8_t)0x07)
#define FTP_CMD_ACK    ((uint8_t)0x08)
typedef uint8_t ftp_cmd_t;

typedef struct {
    ftp_cmd_t cmd;
    int32_t   nbytes;
    char      packet[FTP_PACKETSIZE];
} ftp_chunk_t;

typedef enum {
    FTP_ERR_NONE,
    FTP_ERR_ARGS,
    FTP_ERR_SOCKET,
    FTP_ERR_POLL,
    FTP_ERR_TIMEOUT,
    FTP_ERR_INVALID,
    FTP_ERR_SERVER,
} ftp_err_t;

/**
 * Initialize the FTP implementation to use the given address for outgoing
 * transmissions
 */
void ftp_set_socket(int sockfd, struct sockaddr *addr, socklen_t addr_len);

/**
 * @brief Send arbitrary buffer over the socket
 * Given an arbitrary length buffer, func will break it up into packets
 * and send the chunks through the socket to the address
 */
ftp_err_t ftp_send_data(FILE *infp);

/**
 * recieve FTP_CMD_DATA chunks from sockfd until either a timeout occurs or an
 * FTP_CMD_ERROR is recieved indicating failure or FTP_CMD_TERM is recieved
 * indicating success.
 */
ftp_err_t ftp_recv_data(FILE *outfd, struct sockaddr *out_addr,
                        socklen_t *out_addr_len);

/**
 * Send a single command packet, used for setting up or ending transactions.
 * Use arglen -1 for strings (uses strlen to copy the relevant bit)
 */
ftp_err_t ftp_send_chunk(ftp_cmd_t cmd, const char *arg, ssize_t arglen,
                         int wait_for_ack);

/**
 * Recieve a single command packet, useful for establishing a link (ACK)
 */
ftp_err_t ftp_recv_chunk(ftp_chunk_t *ret, int timeout, int send_ack,
                         struct sockaddr *out_addr, socklen_t *out_addr_len);

/**
 * Print the error message for the last server error to stderr if it exists
 */
void ftp_perror();

#endif
