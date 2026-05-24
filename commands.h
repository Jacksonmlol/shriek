#ifndef COMMANDS_H
#define COMMANDS_H

#include <stddef.h>

#define CMD_PREFIX '/'

#define CMD_HELP "help"
#define CMD_HELP_DESC "Display this help message"
#define CMD_HELP_USAGE "/help [command]"

#define CMD_INFO "info"
#define CMD_INFO_DESC "Display client information"
#define CMD_INFO_USAGE "/info"

#define CMD_COMMANDS "commands"
#define CMD_COMMANDS_DESC "List all available commands"
#define CMD_COMMANDS_USAGE "/commands"

#define CMD_EXIT "exit"
#define CMD_EXIT_DESC "Disconnect from the server"
#define CMD_EXIT_USAGE "/exit"

enum CommandResult {
    CMD_RESULT_NOT_FOUND = 0,
    CMD_RESULT_HANDLED,
    CMD_RESULT_EXIT
};

int handle_command(const char *input, char *response, size_t response_size);

#endif
