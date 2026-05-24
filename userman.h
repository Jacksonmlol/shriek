#ifndef USERMAN_H
#define USERMAN_H

#include <pthread.h>
extern pthread_mutex_t um_mutex;

typedef struct {
    char name[32];
    char addr[64];
    int port;
    int sockfd;
} User;

typedef struct UserNode {
    User user;
    struct UserNode *next;
} UserNode;

typedef struct {
    UserNode *head;
    int count;
} UserManager;

void um_init(UserManager *um);
void um_add(UserManager *um, const char *name, const char *addr, int port, int sockfd);
void um_remove(UserManager *um, int sockfd);
User *um_get_by_sock(UserManager *um, int sockfd);
User *um_get_by_name(UserManager *um, const char *name);
char *um_build_list(UserManager *um);
void um_cleanup(UserManager *um);

#endif
