#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>

int send_json(int sock, const char *data) {
    size_t len = strlen(data);
    char *buf = malloc(len + ENDL + 1);
    if (!buf) return -1;
    memcpy(buf, data, len);
    memcpy(buf + len, END, ENDL);
    buf[len + ENDL] = 0;

    int sent = 0;
    size_t total = len + ENDL;
    while (sent < (int)total) {
        int n = send(sock, buf + sent, total - sent, 0);
        if (n < 0) { free(buf); return -1; }
        sent += n;
    }
    free(buf);
    return sent;
}

char *recv_json(int sock) {
    char *buf = malloc(BUFFSIZE);
    if (!buf) return NULL;
    int pos = 0;
    int done = 0;

    while (!done) {
        int space = BUFFSIZE - pos - 1;
        if (space <= 0) {
            char *bigger = realloc(buf, BUFFSIZE * 2);
            if (!bigger) { free(buf); return NULL; }
            buf = bigger;
            space = (BUFFSIZE * 2) - pos - 1;
        }
        int n = recv(sock, buf + pos, space, 0);
        if (n <= 0) { free(buf); return NULL; }
        pos += n;
        buf[pos] = 0;

        if (pos >= ENDL && memcmp(buf + pos - ENDL, END, ENDL) == 0) {
            buf[pos - ENDL] = 0;
            done = 1;
        }
    }

    char *result = strdup(buf);
    free(buf);
    return result;
}

void logmsg(const char *fmt, ...) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    fprintf(stderr, "[%02d:%02d:%02d] ", tm->tm_hour, tm->tm_min, tm->tm_sec);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}
