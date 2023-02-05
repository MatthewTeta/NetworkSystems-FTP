#pragma once

/**
 * Define the communication protocol shared between the client and the server.
 * This includes the valid command enums, packet size and format,
 * and methods for sending and recieving the data
 */

#include <stdint.h>
#include <sys/socket.h>

#define PACKETSIZE     512
#define FTP_TIMEOUT_MS 3000

/**
 * Client oriented command naming convention
 * Commands:
 *      GET <filename>: move <filename> file from server to client.
 *      PUT <filename>: move <filename> file from cleint to server.
 *      DELETE <filename>: delete <filename> file from server fs.
 *      LS: list the contents of the server filesystem.
 *      // Internal flow commands
 *      CANCEL: Stop the ongoing partial transaction.
 *      TERMINATE: Indiciate that the transaction in finished.
 */
#define FTP_CMD_GET     ((uint8_t)0x01)
#define FTP_CMD_PUT     ((uint8_t)0x02)
#define FTP_CMD_DELETE  ((uint8_t)0x03)
#define FTP_CMD_LS      ((uint8_t)0x04)
#define FTP_CMD_GETPACK ((uint8_t)0x05)
#define FTP_CMD_PUTPACK ((uint8_t)0x06)
#define FTP_CMD_CANCEL  ((uint8_t)0x07)
#define FTP_CMD_TERM    ((uint8_t)0x08)
typedef uint8_t ftp_cmd_t;

typedef struct {
  ftp_cmd_t cmd;
  uint32_t  nbytes;
  char      packet[PACKETSIZE];
} ftp_chunk_t;
