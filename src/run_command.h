#pragma once // run_command.h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "macros.h"
#include <windows.h>

void ClearScreen(void) {
    HANDLE hConsole;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD cellsWritten;
    DWORD consoleSize;
    COORD home = {0, 0};

    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE)
        return;

    if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
        return;

    consoleSize = csbi.dwSize.X * csbi.dwSize.Y;

    FillConsoleOutputCharacter(hConsole, (TCHAR)' ', consoleSize, home, &cellsWritten);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, consoleSize, home, &cellsWritten);
    SetConsoleCursorPosition(hConsole, home);
}


// Helper to detect if /? is present in arguments
int is_help_flag_present(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "/?") == 0) {
            return 1;
        }
    }
    return 0;
}

int run_command(char *cmd, int mode) {
    if (!cmd) return -1;

    // Duplicate string to safely modify it
    char *cmd_copy = strdup(cmd);
    if (!cmd_copy) return -1;

    // Trim leading whitespace
    while (*cmd_copy == ' ' || *cmd_copy == '\t' || *cmd_copy == '\n') cmd_copy++;

    // Tokenize command
    char *args[64] = {0};
    int argc = 0;
    char *rest = cmd_copy;
    char *token;

    while ((token = strsep(&rest, " \t\n")) != NULL) {
        if (*token == '\0') continue;
        args[argc++] = token;
        if (argc >= 63) break;
    }
    args[argc] = NULL;

    if (argc == 0) { free(cmd_copy); return -1; }

    char *command = args[0];

    if (strcmp(command, "exit") == 0) {
        if (is_help_flag_present(argc, args)) {
            printf("Run 'help exit' for information.\n");
        } else {
            int exit_code = 0;
            for (int i = 1; i < argc; i++) {
                if (args[i][0] != '/') { // first non-flag argument
                    exit_code = atoi(args[i]);
                    break;
                }
            }
            free(cmd_copy);
            exit(exit_code);
        }
    } else if (strcmp(command, "cls") == 0) {
        if (is_help_flag_present(argc, args)) {
            printf("Run 'help cls' for information.\n");
        } else {
            ClearScreen();
        }
    }
    else if (strcmp(command, "ver") == 0) {
        if (is_help_flag_present(argc, args)) {
            printf("Run 'help ver' for information.\n");
        } else {
            printf("Microsoft Windows [Version 10.0.26220.1337]\n");
        }
    }
    else if (strcmp(command, "help") == 0) {
        if (is_help_flag_present(argc, args) || argc == 1) {
            // General help
            printf(
                "Available commands:\n\n"
                "help - Show help information\n"
                "ver  - Show version information\n"
                "cls  - Clear the screen\n"
                "exit - Exit the program\n\n"
                "Type help <command> for help on a specific command.\n"
            );
        } else {
            // Specific help
            char *sub = args[1];
            if (strcmp(sub, "exit") == 0) printf("exit [code] - Exits the program with optional exit code.\n");
            else if (strcmp(sub, "ver") == 0) printf("ver - Displays the program version.\n");
            else if (strcmp(sub, "help") == 0) printf("help [command] - Shows help information.\n");
            else if (strcmp(sub, "cls") == 0) printf("cls - Clears the screen.\n");
            else printf("No help available for: %s\n", sub);
        }
    }
    else {
        fprintf(stderr, "bad command: %s\n", command);
        free(cmd_copy);
        return 9009;
    }

    free(cmd_copy);
    return 0;
}
