#include "userman.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

pthread_mutex_t um_mutex = PTHREAD_MUTEX_INITIALIZER;

void um_init(UserManager *um) {
    um->head = NULL;
    um->count = 0;
}

void um_add(UserManager *um, const char *name, const char *addr, int port, int sockfd) {
    pthread_mutex_lock(&um_mutex);
    UserNode *node = malloc(sizeof(UserNode));
    if (!node) {
        pthread_mutex_unlock(&um_mutex);
        return;
    }
    strncpy(node->user.name, name, 31);
    node->user.name[31] = 0;
    strncpy(node->user.addr, addr, 63);
    node->user.addr[63] = 0;
    node->user.port = port;
    node->user.sockfd = sockfd;
    node->next = um->head;
    um->head = node;
    um->count++;
    pthread_mutex_unlock(&um_mutex);
}

void um_remove(UserManager *um, int sockfd) {
    pthread_mutex_lock(&um_mutex);
    UserNode **curr = &um->head;
    while (*curr) {
        if ((*curr)->user.sockfd == sockfd) {
            UserNode *tmp = *curr;
            *curr = tmp->next;
            free(tmp);
            um->count--;
            pthread_mutex_unlock(&um_mutex);
            return;
        }
        curr = &(*curr)->next;
    }
    pthread_mutex_unlock(&um_mutex);
}

User *um_get_by_sock(UserManager *um, int sockfd) {
    pthread_mutex_lock(&um_mutex);
    UserNode *curr = um->head;
    while (curr) {
        if (curr->user.sockfd == sockfd) {
            pthread_mutex_unlock(&um_mutex);
            return &curr->user;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&um_mutex);
    return NULL;
}

User *um_get_by_name(UserManager *um, const char *name) {
    pthread_mutex_lock(&um_mutex);
    UserNode *curr = um->head;
    while (curr) {
        if (strcmp(curr->user.name, name) == 0) {
            pthread_mutex_unlock(&um_mutex);
            return &curr->user;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&um_mutex);
    return NULL;
}

char *um_build_list(UserManager *um) {
    pthread_mutex_lock(&um_mutex);
    char *buf = malloc(16384);
    if (!buf) { pthread_mutex_unlock(&um_mutex); return NULL; }
    buf[0] = '[';
    int off = 1;
    UserNode *curr = um->head;
    while (curr) {
        char *ename = malloc(strlen(curr->user.name) * 2 + 1);
        int j = 0;
        for (const char *p = curr->user.name; *p; p++) {
            if (*p == '"' || *p == '\\') ename[j++] = '\\';
            ename[j++] = *p;
        }
        ename[j] = 0;
        off += snprintf(buf + off, 16384 - off,
            "{\"name\":\"%s\",\"sid\":[\"%s\",%d]}",
            ename, curr->user.addr, curr->user.port);
        free(ename);
        if (curr->next) off += snprintf(buf + off, 16384 - off, ",");
        curr = curr->next;
    }
    snprintf(buf + off, 16384 - off, "]");
    pthread_mutex_unlock(&um_mutex);
    return buf;
}

void um_cleanup(UserManager *um) {
    pthread_mutex_lock(&um_mutex);
    UserNode *curr = um->head;
    while (curr) {
        UserNode *tmp = curr;
        curr = curr->next;
        free(tmp);
    }
    um->head = NULL;
    um->count = 0;
    pthread_mutex_unlock(&um_mutex);
}
