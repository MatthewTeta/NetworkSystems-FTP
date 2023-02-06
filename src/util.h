#ifndef FTP_UTIL
#define FTP_UTIL

#include <stdio.h>
#include <stdint.h>

void print_hex(uint8_t *buf, size_t len) {
    int i, c;
    int linelen = 16;
    int nlines = len / linelen + !!(len % linelen);
    for (i = 0; i < nlines; ++i) {
        printf("[%d]:\t0x", i);
        for (c = 0; c < linelen; ++c) {
            printf("%02X ", 0xFF & buf[i * linelen + c]);
        }
        puts("");
    }
}

#endif
