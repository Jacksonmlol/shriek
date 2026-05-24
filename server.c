#include "util.h"
#include "userman.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

static UserManager userman;
static int server_sock;
static volatile int running = 1;

typedef struct {
    int sockfd;
    struct sockaddr_in addr;
} ClientInfo;

static pthread_mutex_t broadcast_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast(const char *json_str, int exclude) {
    // this is kinda janky but whatever
    pthread_mutex_lock(&broadcast_mutex);
    pthread_mutex_lock(&um_mutex); // from userman.c
    UserNode *curr = userman.head;
    while (curr) {
        if (curr->user.sockfd != exclude) {
            send_json(curr->user.sockfd, json_str);
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&um_mutex);
    pthread_mutex_unlock(&broadcast_mutex);
}

void send_room_update(void) {
    char *list = um_build_list(&userman);
    if (!list) return;
    char *msg = jarr("room_update", "user_list", list);
    free(list);
    if (msg) {
        broadcast(msg, -1);
        jfree(msg);
    }
}

void handle_message(const char *raw, int client_sock) {
    char *type = jstr(raw, "type");
    if (!type) {
        logmsg("got message with no type, ignoring");
        return;
    }

    if (strcmp(type, "send_message") == 0) {
        char *msg = jstr(raw, "message");
        if (msg) {
            if (strlen(msg) > 0) {
                User *u = um_get_by_sock(&userman, client_sock);
                if (u) {
                    char *out = jobj("user_message", "from", u->name, "message", msg, NULL);
                    if (out) {
                        broadcast(out, -1);
                        jfree(out);
                    }
                }
            }
            free(msg);
        }

    } else if (strcmp(type, "join") == 0) {
        char *username = jstr(raw, "username");
        if (username && strlen(username) > 0 && !um_get_by_name(&userman, username)) {
            struct sockaddr_in addr;
            socklen_t addrlen = sizeof(addr);
            getpeername(client_sock, (struct sockaddr *)&addr, &addrlen);
            um_add(&userman, username, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), client_sock);

            char *accept = jobj("join_accept", NULL);
            if (accept) {
                send_json(client_sock, accept);
                jfree(accept);
            }

            char joinmsg[128];
            snprintf(joinmsg, sizeof(joinmsg), "%s has joined the chat.", username);
            char *sys = jobj("system_message", "message", joinmsg, NULL);
            if (sys) {
                broadcast(sys, -1);
                jfree(sys);
            }
            send_room_update();
            logmsg("user joined: %s", username);
        } else {
            char *deny = jobj("join_deny", "message", "Invalid username or name taken", NULL);
            if (deny) {
                send_json(client_sock, deny);
                jfree(deny);
            }
            logmsg("rejected join: %s", username ? username : "(null)");
        }
        if (username) free(username);

    } else {
        logmsg("unknown message type: %s", type);
    }

    free(type);
}

void *client_thread(void *arg) {
    ClientInfo *ci = (ClientInfo *)arg;
    int sock = ci->sockfd;
    char addr_str[64];
    int client_port = ntohs(ci->addr.sin_port);
    inet_ntop(AF_INET, &ci->addr.sin_addr, addr_str, sizeof(addr_str));
    logmsg("client connected: %s:%d", addr_str, client_port);
    free(ci);

    while (1) {
        char *data = recv_json(sock);
        if (!data) break;
        handle_message(data, sock);
        free(data);
    }

    // cleanup on disconnect
    User *u = um_get_by_sock(&userman, sock);
    if (u) {
        char leavemsg[128];
        snprintf(leavemsg, sizeof(leavemsg), "%s has left the chat.", u->name);
        char *sys = jobj("system_message", "message", leavemsg, NULL);
        if (sys) {
            broadcast(sys, -1);
            jfree(sys);
        }
        logmsg("user left: %s", u->name);
        um_remove(&userman, sock);
        send_room_update();
    }

    close(sock);
    logmsg("connection closed: %s:%d", addr_str, client_port);
    return NULL;
}

void handle_sigint(int sig) {
    (void)sig;
    logmsg("shutting down...");
    running = 0;
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    um_init(&userman);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_sock);
        return 1;
    }

    printf("\n");
    printf("  .d8888. db   db d8888b. d888888b d88888b db   dD \n");
    printf("  88'  YP 88   88 88  `8D   `88'   88'     88 ,8P' \n");
    printf("  `8bo.   88ooo88 88oobY'    88    88ooooo 88,8P   \n");
    printf("    `Y8b. 88~~~88 88`8b      88    88~~~~~ 88`8b   \n");
    printf("  db   8D 88   88 88 `88.   .88.   88.     88 `88. \n");
    printf("  `8888Y' YP   YP 88   YD Y888888P Y88888P YP   YP \n");
    printf("\n");
    printf("  Shriek server starting on port %d...\n", PORT);

    listen(server_sock, 10);

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client < 0) {
            if (running) perror("accept");
            break;
        }

        ClientInfo *ci = malloc(sizeof(ClientInfo));
        if (!ci) { close(client); continue; }
        ci->sockfd = client;
        ci->addr = client_addr;

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, ci);
        pthread_detach(tid);
    }

    logmsg("cleaning up...");
    um_cleanup(&userman);
    close(server_sock);
    logmsg("goodbye!");
    return 0;
}
