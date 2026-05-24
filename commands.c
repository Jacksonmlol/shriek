#include "commands.h"
#include <string.h>
#include <stdio.h>

struct Command {
    const char *name;
    const char *desc;
    const char *usage;
};

static const struct Command cmd_table[] = {
    {CMD_HELP,     CMD_HELP_DESC,     CMD_HELP_USAGE},
    {CMD_INFO,     CMD_INFO_DESC,     CMD_INFO_USAGE},
    {CMD_COMMANDS, CMD_COMMANDS_DESC, CMD_COMMANDS_USAGE},
    {CMD_EXIT,     CMD_EXIT_DESC,     CMD_EXIT_USAGE},
};

#define CMD_COUNT (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_index(const char *name) {
    for (size_t i = 0; i < CMD_COUNT; i++) {
        if (strcmp(cmd_table[i].name, name) == 0)
            return (int)i;
    }
    return -1;
}

int handle_command(const char *input, char *response, size_t response_size) {
    if (!input || input[0] != CMD_PREFIX)
        return CMD_RESULT_NOT_FOUND;

    const char *cmd_name = input + 1;

    const char *arg = NULL;
    for (const char *p = cmd_name; *p; p++) {
        if (*p == ' ') {
            arg = p + 1;
            break;
        }
    }

    size_t name_len;
    if (arg)
        name_len = (size_t)(arg - cmd_name - 1);
    else
        name_len = strlen(cmd_name);

    char name[64];
    if (name_len >= sizeof(name))
        return CMD_RESULT_NOT_FOUND;
    memcpy(name, cmd_name, name_len);
    name[name_len] = 0;

    if (name_len == 0)
        return CMD_RESULT_NOT_FOUND;

    int idx = cmd_index(name);
    if (idx < 0) {
        snprintf(response, response_size,
            "Unknown command. " CMD_HELP_USAGE " for available commands.");
        return CMD_RESULT_HANDLED;
    }

    if (strcmp(name, CMD_EXIT) == 0) {
        return CMD_RESULT_EXIT;
    }

    if (strcmp(name, CMD_HELP) == 0) {
        if (arg) {
            int aidx = cmd_index(arg);
            if (aidx < 0) {
                snprintf(response, response_size, "Unknown command '%s'.", arg);
            } else {
                snprintf(response, response_size, "%s -- %s",
                    cmd_table[aidx].usage, cmd_table[aidx].desc);
            }
        } else {
            size_t pos = 0;
            for (size_t i = 0; i < CMD_COUNT && pos < response_size; i++) {
                int n = snprintf(response + pos, response_size - pos,
                    "%s -- %s\n", cmd_table[i].usage, cmd_table[i].desc);
                if (n > 0)
                    pos += (size_t)n;
                if (pos >= response_size)
                    break;
            }
            if (pos > 0 && response[pos - 1] == '\n')
                response[pos - 1] = 0;
        }
        return CMD_RESULT_HANDLED;
    }

    if (strcmp(name, CMD_COMMANDS) == 0) {
        size_t pos = 0;
        for (size_t i = 0; i < CMD_COUNT && pos < response_size; i++) {
            int n = snprintf(response + pos, response_size - pos,
                "%s\n", cmd_table[i].usage);
            if (n > 0)
                pos += (size_t)n;
            if (pos >= response_size)
                break;
        }
        if (pos > 0 && response[pos - 1] == '\n')
            response[pos - 1] = 0;
        return CMD_RESULT_HANDLED;
    }

    if (strcmp(name, CMD_INFO) == 0) {
        snprintf(response, response_size,
            "Shriek Chat Client -- original owner: boyne, C version owner: Jackson. " CMD_HELP_USAGE " for commands.");
        return CMD_RESULT_HANDLED;
    }

    return CMD_RESULT_NOT_FOUND;
}
