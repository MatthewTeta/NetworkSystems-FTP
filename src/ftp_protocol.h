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
 *      CANCEL : Stop the ongoing partial transaction.
 *      TERM : Indiciate that the transaction in finished.
 */
#define FTP_CMD_GET    ((uint8_t)0x01)
#define FTP_CMD_PUT    ((uint8_t)0x02)
#define FTP_CMD_DELETE ((uint8_t)0x03)
#define FTP_CMD_LS     ((uint8_t)0x04)
#define FTP_CMD_DATA   ((uint8_t)0x05)
#define FTP_CMD_CANCEL ((uint8_t)0x06)
#define FTP_CMD_TERM   ((uint8_t)0x07)
typedef uint8_t ftp_cmd_t;

typedef struct {
  ftp_cmd_t cmd;
  uint32_t  nbytes;
  uint8_t   packet[FTP_PACKETSIZE];
} ftp_chunk_t;

typedef enum {
  FTP_ERR_NONE,
  FTP_ERR_ARGS,
  FTP_ERR_SOCKET,
  FTP_ERR_POLL,
  FTP_ERR_TIMEOUT,
  FTP_ERR_INVALID,
} ftp_err_t;

/**
 * @brief Send arbitrary buffer over the socket
 * Given an arbitrary length buffer, func will break it up into packets
 * and send the chunks through the socket to the address
 */
ftp_err_t ftp_send_data(int sockfd, const uint8_t *buf, size_t n,
                        const struct sockaddr *addr, socklen_t addr_len);

/**
 * recieve FTP_CMD_DATA chunks from sockfd until either a timeout occurs or an
 * FTP_CMD_CANCEL is recieved indicating failure or FTP_CMD_TERM is recieved
 * indicating success.
 */
ftp_err_t ftp_recv_data(int sockfd, int outfd);

/**
 * Send a single command packet, used for setting up or ending transactions
 */
ftp_err_t ftp_send_cmd(int sockfd, ftp_cmd_t cmd, const uint8_t *arg,
                       size_t arglen, const struct sockaddr *addr,
                       socklen_t addr_len);

/**
 * Recieve a single command packet, useful for establishing a link (ACK)
 */
ftp_err_t ftp_recv_cmd(int sockfd, ftp_chunk_t *ret);

#endif
