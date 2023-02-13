#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftp_protocol.h"
#include "util.h"

// Private Variables
int              sockfd   = 0;
struct sockaddr *addr     = NULL;
socklen_t        addr_len = 0;
// .text chunk for error printing
ftp_chunk_t _ftp_error = {
    .cmd    = 0,
    .nbytes = 0,
};

// Functions
void ftp_set_socket(int _sockfd, struct sockaddr *_addr, socklen_t _addr_len) {
    sockfd   = _sockfd;
    addr     = _addr;
    addr_len = _addr_len;
}

ftp_err_t ftp_send_data(FILE *infp) {
    if (sockfd <= 0 || !infp || !addr) {
        return FTP_ERR_ARGS;
    }

    ftp_err_t rv;
    size_t    nread;
    char      buf[FTP_PACKETSIZE];

    // Fracture buf into chunks until all are sent
    while (0 != (nread = fread(buf, 1, FTP_PACKETSIZE, infp))) {
        // printf("CHUNK: %lu\n", n_remaining);
        if (FTP_ERR_NONE != (rv = ftp_send_chunk(FTP_CMD_DATA, buf, nread, 1)))
            return rv;

        // TODO:
        // (Synchronous) wait for ACK from recieving party
        // rv = ftp_recv_chunk(sockfd)
    }

    // Send TERM to indicate file is done
    ftp_send_chunk(FTP_CMD_TERM, NULL, 0, 1);

    return FTP_ERR_NONE;
}

ftp_err_t ftp_recv_data(FILE *outfd, struct sockaddr *out_addr,
                        socklen_t *out_addr_len) {
    if (sockfd <= 0)
        return -1;

    ftp_err_t   rv;
    ftp_chunk_t chunk = {
        .cmd = FTP_CMD_DATA,
    };

    while (1) {
        // Receive chunks
        if (FTP_ERR_NONE != (rv = ftp_recv_chunk(&chunk, FTP_TIMEOUT_MS, 1,
                                                 out_addr, out_addr_len)))
            return rv;
        // Determine what to do with the recieved chunk data
        switch (chunk.cmd) {
        case FTP_CMD_DATA:
            if (outfd) {
                fwrite(chunk.packet, 1, (size_t)chunk.nbytes, outfd);
            }
            break;
        case FTP_CMD_ERROR:
            // Store the error in private variable
            memcpy(&_ftp_error, &chunk, sizeof(ftp_chunk_t));
            ftp_perror();
            return FTP_ERR_SERVER;
        case FTP_CMD_TERM:
            // Expected value
            return FTP_ERR_NONE;
        default:
            return FTP_ERR_INVALID;
        }
        // printf("chunk [%d]: CMD 0x%02X : SIZE %u\n", i++, chunk.cmd,
        // chunk.nbytes); print_hex(chunk.packet, chunk.nbytes);
        // TODO: Write to outfd if not NULL
    }

    return FTP_ERR_INVALID;
}

ftp_err_t ftp_send_chunk(ftp_cmd_t cmd, const char *arg, ssize_t arglen,
                         int wait_for_ack) {
    // Used to keep packet responses synchronized (possible unnecessary sinc
    // everything will be synchronous)
    // static uint32_t packet_num = 0;

    if (sockfd <= 0 || arglen > FTP_PACKETSIZE || arglen < -1 || !addr ||
        addr_len == 0)
        return FTP_ERR_ARGS;

    ftp_chunk_t chunk, ack;

    // Construct a data packet
    bzero(&chunk, sizeof(chunk)); // This is technically unnecessary
    chunk.cmd = cmd;
    // chunk.packet_num = packet_num++;
    // printf("Send CMD: %02X:%02X\n", chunk.cmd, cmd);
    chunk.nbytes = arglen;

    // Set the packet field to zero unless an argument is provided
    if (arg != NULL) {
        if (arglen == -1) {
            strncpy(chunk.packet, arg, FTP_PACKETSIZE);
        } else {
            memcpy((void *)(&chunk.packet), (void *)arg, (size_t)chunk.nbytes);
        }
    } else {
        bzero((void *)(&chunk.packet), FTP_PACKETSIZE);
    }

    // Send the packet until all the data has been sent
    ssize_t nsent = 0;
    size_t  ntot  = 0;
    // Keep calling sendto until the full length of the packet is transmitted
    while (ntot != sizeof(ftp_chunk_t)) {
        // TODO: Convert to network byte order
        nsent = sendto(sockfd, (void *)((&chunk) + ntot),
                       sizeof(ftp_chunk_t) - ntot, 0, addr, addr_len);
        if (nsent < 0) {
            fprintf(stderr, "Error sending data in ftp_send_buf\n");
            return FTP_ERR_SOCKET;
        }
        ntot += nsent;
    }

    // Wait for ACK from reciever
    if (wait_for_ack) {
        ftp_err_t rv = ftp_recv_chunk(&ack, FTP_TIMEOUT_MS, 0, NULL, NULL);
        // Expect ACK or ERROR
        switch (rv) {
        case FTP_ERR_NONE:
            break;
        case FTP_ERR_TIMEOUT:
            // TODO: Resend chun
            return rv;
        default:
            return rv;
        }

        // Check whether we got an ack or an error with the same packet_num
        switch (ack.cmd) {
        case FTP_CMD_ACK:
            // Check that packet number is matching and return success
            // if (chunk.packet_num != ack.packet_num) {
            //   fprintf(stderr, "Packet numbers are out of sync!");
            //   return FTP_ERR_INVALID;
            // }
            return FTP_ERR_NONE;
        case FTP_CMD_ERROR:
            // Store the error in private variable
            memcpy(&_ftp_error, &chunk, sizeof(ftp_chunk_t));
            ftp_perror();
            return FTP_ERR_SERVER;
        default:
            fprintf(stderr,
                    "Expected ACK or ERR. Invalid CMD (%02X) recieved\n",
                    ack.cmd);
            return FTP_ERR_INVALID;
        }
    }
    return FTP_ERR_NONE;
}

ftp_err_t ftp_recv_chunk(ftp_chunk_t *ret, int timeout, int send_ack,
                         struct sockaddr *out_addr, socklen_t *out_addr_len) {
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
        int poll_rv = poll(&fds, 1, timeout);
        if (poll_rv < 0) {
            fprintf(stderr, "Error while polling in ftp_recv_chunk\n");
            return FTP_ERR_POLL;
        } else if (poll_rv == 0) {
            return FTP_ERR_TIMEOUT;
        } else {
            // Event happened
            if (revents & (POLLERR | POLLNVAL)) {
                fprintf(stderr, 
                    "There was an error with the file descriptor when polling\n");
                return FTP_ERR_POLL;
            }
            // There is data to be read from the pipe
            // Append to the buffer by using pointer arithmetic
            int n =
                recvfrom(sockfd, (void *)(ret + nrec),
                         sizeof(ftp_chunk_t) - nrec, 0, out_addr, out_addr_len);
            // printf("n = %d\n", n);
            if (n < 0) {
                fprintf(stderr, "ERROR in recvfrom\n");
                return FTP_ERR_SOCKET;
            }
            nrec += n;
        }
    }

    // Send ACK packet
    if (send_ack && ret->cmd != FTP_CMD_ACK && ret->cmd != FTP_CMD_ERROR) {
        ftp_send_chunk(FTP_CMD_ACK, NULL, 0, 0);
    }

    return FTP_ERR_NONE;
}

void ftp_perror() {
    if (_ftp_error.cmd == FTP_CMD_ERROR) {
        fprintf(stderr, "%s\n", _ftp_error.packet);
    }
}
