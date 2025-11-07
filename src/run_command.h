#pragma once // run_command.h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "macros.h"

int run_command(char *cmd, int mode) {
    if (!cmd) return -1;

    // Duplicate the string to safely modify it
    char *cmd_copy = strdup(cmd);
    if (!cmd_copy) return -1;

    // Trim leading whitespace
    while (*cmd_copy == ' ' || *cmd_copy == '\t' || *cmd_copy == '\n') cmd_copy++;

    // Prepare argument array
    char *args[64] = {0}; // initialize to NULL
    int argc = 0;

    char *rest = cmd_copy;
    char *token;

    // Tokenize by space, tab, or newline
    while ((token = strsep(&rest, " \t\n")) != NULL) {
        if (*token == '\0') continue; // skip empty tokens
        args[argc++] = token;
        if (argc >= 63) break; // avoid overflow
    }
    args[argc] = NULL;

    if (argc == 0) { // no command entered
        free(cmd_copy);
        return -1;
    }

    char *command = args[0];

    // Handle internal commands
    if (strcmp(command, "exit") == 0) {
        int exit_code = 0;
        if (argc > 1) exit_code = atoi(args[1]); // use argument if provided
        free(cmd_copy);
        exit(exit_code);
    }
    else if (strcmp(command, "ver") == 0) {
        printf("Microsoft Windows [Version 10.0.26220.1337]\n");
    }
    else {
        fprintf(stderr, "bad command: %s\n", command);
        free(cmd_copy);
        return 9009;
    }

    free(cmd_copy);
    return 0;
}
