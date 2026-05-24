#include "util.h"
#include "json.h"
#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <ncurses.h>

#define MAX_MESSAGES 1000

static int sockfd = -1;
static volatile int connected = 0;
static volatile int running = 1;

static pthread_mutex_t msg_mutex = PTHREAD_MUTEX_INITIALIZER;
static char **messages = NULL;
static int msg_count = 0;
static int msg_cap = 0;

static char **userlist = NULL;
static int user_count = 0;

static int pipe_fds[2];

void add_message(const char *sender, const char *text) {
    pthread_mutex_lock(&msg_mutex);
    if (msg_count >= msg_cap) {
        int newcap = msg_cap ? msg_cap * 2 : 64;
        char **newmsgs = realloc(messages, newcap * sizeof(char *));
        if (!newmsgs) { pthread_mutex_unlock(&msg_mutex); return; }
        messages = newmsgs;
        msg_cap = newcap;
    }
    char *line = malloc(strlen(sender) + strlen(text) + 4);
    if (line) {
        sprintf(line, "%s|%s", sender, text);
        messages[msg_count++] = line;
    }
    pthread_mutex_unlock(&msg_mutex);
}

void clear_users(void) {
    for (int i = 0; i < user_count; i++) free(userlist[i]);
    user_count = 0;
}

void add_user(const char *name) {
    userlist = realloc(userlist, (user_count + 1) * sizeof(char *));
    userlist[user_count++] = strdup(name);
}

void *net_thread(void *arg) {
    (void)arg;
    while (connected) {
        char *data = recv_json(sockfd);
        if (!data) {
            connected = 0;
            break;
        }

        char *type = jstr(data, "type");
        if (!type) { free(data); continue; }

        if (strcmp(type, "user_message") == 0) {
            char *from = jstr(data, "from");
            char *msg = jstr(data, "message");
            if (from && msg) add_message(from, msg);
            free(from);
            free(msg);

        } else if (strcmp(type, "system_message") == 0) {
            char *msg = jstr(data, "message");
            if (msg) add_message("* SYSTEM *", msg);
            free(msg);

        } else if (strcmp(type, "room_update") == 0) {
            clear_users();
            // parse user list from the array
            // format: {"type":"room_update","user_list":[{"name":"x","sid":[...]},...]}
            char *ptr = data;
            char *ul = strstr(ptr, "\"user_list\":");
            if (ul) {
                ul += 11;
                while (*ul && *ul != '[') ul++;
                if (*ul == '[') {
                    ul++;
                    while (*ul && *ul != ']') {
                        char *n = jstr(ul, "name");
                        if (n) {
                            add_user(n);
                            free(n);
                        }
                        // skip to next object
                        while (*ul && *ul != '}') ul++;
                        if (*ul) ul++;
                        while (*ul && (*ul == ',' || *ul == ' ')) ul++;
                    }
                }
            }

        } else if (strcmp(type, "join_accept") == 0) {
            // cool, we're in
        } else if (strcmp(type, "join_deny") == 0) {
            char *msg = jstr(data, "message");
            add_message("* ERROR *", msg ? msg : "Connection denied");
            free(msg);
            connected = 0;
        }

        free(type);
        free(data);
        write(pipe_fds[1], "x", 1); // wake up the UI
    }
    return NULL;
}

void draw_ui(int width, int input_y) {
    int msg_width = width * 3 / 4;

    // Draw message area
    for (int i = 0; i < input_y - 1; i++) {
        move(i, 0);
        clrtoeol();
    }

    int start = msg_count > (input_y - 2) ? msg_count - (input_y - 2) : 0;
    int line = 0;
    for (int i = start; i < msg_count && line < input_y - 2; i++, line++) {
        if (messages[i]) {
            char *sep = strchr(messages[i], '|');
            if (sep) {
                *sep = 0;
                attron(A_BOLD);
                mvprintw(line, 0, "%s", messages[i]);
                attroff(A_BOLD);
                mvprintw(line, strlen(messages[i]) + 1, "%s", sep + 1);
                *sep = '|';
            } else {
                mvprintw(line, 0, "%s", messages[i]);
            }
        }
        clrtoeol();
    }

    // Draw separator and user list
    for (int i = 0; i < input_y - 1; i++) {
        mvaddch(i, msg_width, '|');
    }

    attron(A_BOLD);
    mvprintw(0, msg_width + 2, "Users (%d)", user_count);
    attroff(A_BOLD);
    for (int i = 0; i < user_count && i + 1 < input_y - 1; i++) {
        mvprintw(i + 1, msg_width + 2, "%s", userlist[i]);
        clrtoeol();
    }

    // Draw input line
    move(input_y, 0);
    clrtoeol();
}

void send_chat(const char *text) {
    if (!connected || strlen(text) == 0) return;
    char *msg = jobj("send_message", "message", text, NULL);
    if (msg) {
        send_json(sockfd, msg);
        jfree(msg);
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    if (argc < 2) {
        printf("Usage: %s <server> [port] [username]\n", argv[0]);
        return 1;
    }

    const char *server_host = argv[1];
    int port = argc > 2 ? atoi(argv[2]) : PORT;
    char username[64] = {0};

    if (argc > 3) {
        strncpy(username, argv[3], 63);
    } else {
        printf("Username: ");
        if (!fgets(username, sizeof(username), stdin)) return 1;
        username[strcspn(username, "\n")] = 0;
    }

    if (strlen(username) < 3) {
        printf("Username must be at least 3 characters.\n");
        return 1;
    }

    // Connect
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    struct hostent *he = gethostbyname(server_host);
    if (!he) {
        fprintf(stderr, "Could not resolve host: %s\n", server_host);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    connected = 1;

    // Send join
    char *join = jobj("join", "username", username, NULL);
    if (join) {
        send_json(sockfd, join);
        jfree(join);
    }

    // Setup pipe for UI wakeup
    pipe(pipe_fds);

    // Start network thread
    pthread_t tid;
    pthread_create(&tid, NULL, net_thread, NULL);

    // ncurses setup
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    int height, width;
    char input[1024] = {0};
    int input_pos = 0;
    int msg_y = 0;

    while (running && connected) {
        getmaxyx(stdscr, height, width);
        msg_y = height - 1;

        draw_ui(width, msg_y);

        // Draw input prompt
        mvprintw(msg_y, 0, "> %s", input);
        clrtoeol();
        move(msg_y, 2 + (input_pos > width - 2 ? width - 2 : input_pos));
        refresh();

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(pipe_fds[0], &rfds);
        struct timeval tv = {0, 100000}; // 100ms

        select(pipe_fds[0] + 1, &rfds, NULL, NULL, &tv);

        if (FD_ISSET(pipe_fds[0], &rfds)) {
            char tmp;
            read(pipe_fds[0], &tmp, 1);
        }

        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            int ch = getch();
            if (ch == '\n') {
                if (input_pos > 0) {
                    input[input_pos] = 0;
                    if (input[0] == CMD_PREFIX) {
                        char response[4096];
                        int res = handle_command(input, response, sizeof(response));
                        if (res == CMD_RESULT_EXIT) {
                            running = 0;
                        } else if (res == CMD_RESULT_HANDLED) {
                            add_message("* COMMAND *", response);
                        }
                    } else {
                        send_chat(input);
                    }
                    input_pos = 0;
                    input[0] = 0;
                }
            } else if (ch == 127 || ch == KEY_BACKSPACE || ch == '\b') {
                if (input_pos > 0) input[--input_pos] = 0;
            } else if (ch >= 32 && ch <= 126 && input_pos < (int)sizeof(input) - 1) {
                input[input_pos++] = ch;
                input[input_pos] = 0;
            }
        }
    }

    // Cleanup
    endwin();
    connected = 0;
    pthread_join(tid, NULL);
    close(sockfd);
    close(pipe_fds[0]);
    close(pipe_fds[1]);

    for (int i = 0; i < msg_count; i++) free(messages[i]);
    free(messages);
    for (int i = 0; i < user_count; i++) free(userlist[i]);
    free(userlist);

    return 0;
}
