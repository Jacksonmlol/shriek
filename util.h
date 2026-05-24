#ifndef UTIL_H
#define UTIL_H

#define END "\xe2\x90\x83"
#define ENDL 3
#define BUFFSIZE 8192
#define PORT 44375

int send_json(int sock, const char *data);
char *recv_json(int sock);
void logmsg(const char *fmt, ...);

#endif
