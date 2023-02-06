#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftp_protocol.h"
#include "util.h"

ftp_err_t ftp_send_data(int sockfd, const uint8_t *buf, size_t n,
                        const struct sockaddr *addr, socklen_t addr_len) {
  if (!buf || !addr || n == 0) {
    return FTP_ERR_ARGS;
  }

  ftp_err_t rv;
  size_t    n_remaining = n;
  // uint8_t     *p           = buf;

  // Fracture buf into chunks until all are sent
  while (n_remaining) {
    printf("CHUNK: %lu\n", n_remaining);
    size_t packet_size =
        n_remaining >= FTP_PACKETSIZE ? FTP_PACKETSIZE : n_remaining;
    if (FTP_ERR_NONE != (rv = ftp_send_chunk(sockfd, FTP_CMD_DATA, buf,
                                             packet_size, addr, addr_len)))
      return rv;
    buf += packet_size;
    n_remaining -= packet_size;
  }
  // Send a termination command
  if (FTP_ERR_NONE !=
      (rv = ftp_send_chunk(sockfd, FTP_CMD_TERM, NULL, 0, addr, addr_len)))
    return rv;

  return FTP_ERR_NONE;
}

ftp_err_t ftp_recv_data(int sockfd, int outfd, struct sockaddr *addr,
                        socklen_t *addrlen) {
  if (sockfd <= 0 || outfd <= 0)
    return -1;

  ftp_err_t   rv;
  ftp_chunk_t chunk = {
      .cmd = FTP_CMD_DATA,
  };

  int i = 0;
  while (chunk.cmd == FTP_CMD_DATA) {
    // Receive chunks
    if (FTP_ERR_NONE !=
        (rv = ftp_recv_chunk(sockfd, &chunk, FTP_TIMEOUT_MS, addr, addrlen)))
      return rv;
    printf("chunk [%d]: CMD 0x%02X : SIZE %u\n", i++, chunk.cmd, chunk.nbytes);
    print_hex(chunk.packet, chunk.nbytes);
  }
  return 0;
}

ftp_err_t ftp_send_chunk(int sockfd, ftp_cmd_t cmd, const uint8_t *arg,
                         size_t arglen, const struct sockaddr *addr,
                         socklen_t addr_len) {
  if (sockfd <= 0 || arglen > FTP_PACKETSIZE || !addr || addr_len == 0)
    return FTP_ERR_ARGS;

  ftp_chunk_t chunk;

  // Construct a data packet
  bzero(&chunk, sizeof(chunk)); // This is technically unnecessary
  chunk.cmd = cmd;
  // printf("Send CMD: %02X:%02X\n", chunk.cmd, cmd);
  chunk.nbytes = arglen;

  // Set the packet field to zero unless an argument is provided
  if (arg != NULL) {
    memcpy((void *)(&chunk.packet), (void *)arg, (size_t)chunk.nbytes);
  } else {
    bzero((void *)(&chunk.packet), FTP_PACKETSIZE);
  }

  // Send the packet until all the data has been sent
  ssize_t nsent = 0;
  size_t  ntot  = 0;
  // Keep calling sendto until the full length of the packet is transmitted
  while (ntot != sizeof(ftp_chunk_t)) {
    // TODO: Convert to network byte order
    nsent = sendto(sockfd, (void *)((uint8_t *)(&chunk) + ntot),
                   sizeof(ftp_chunk_t) - ntot, 0, addr, addr_len);
    if (nsent < 0) {
      perror("Error sending data in ftp_send_buf");
      return FTP_ERR_SOCKET;
    }
    ntot += nsent;
  }
  return FTP_ERR_NONE;
}

ftp_err_t ftp_recv_chunk(int sockfd, ftp_chunk_t *ret, int timeout,
                         struct sockaddr *addr, socklen_t *addrlen) {
  if (sockfd <= 0 || !ret)
    return FTP_ERR_ARGS;

  size_t nrec = 0;

  // We are assuming only one server, so we don't really need to check the
  // information stored into the serveraddr

  // Recieve packets until we have a whole chunk
  while (nrec < sizeof(ftp_chunk_t)) {
    // Poll to allow for a timeout
    short         revents = 0;
    struct pollfd fds     = {
            .fd      = sockfd,
            .events  = POLLIN,
            .revents = revents,
    };
    int rv = poll(&fds, 1, timeout);
    if (rv < 0) {
      perror("Error while polling in ftp_recv_chunk");
      return FTP_ERR_POLL;
    } else if (rv == 0) {
      return FTP_ERR_TIMEOUT;
    } else {
      // Event happened
      if (revents & (POLLERR | POLLNVAL)) {
        perror("There was an error with the file descriptor when polling");
        return FTP_ERR_POLL;
      }
      // There is data to be read from the pipe
      // Append to the buffer by using pointer arithmetic
      int n = recvfrom(sockfd, (void *)(ret + nrec), sizeof(ftp_chunk_t) - nrec,
                       0, addr, addrlen);
      // printf("n = %d\n", n);
      if (n < 0) {
        perror("ERROR in recvfrom");
        return FTP_ERR_SOCKET;
      }
      nrec += n;
    }
  }

  return FTP_ERR_NONE;
}
