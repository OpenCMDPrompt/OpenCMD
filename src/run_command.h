#pragma once

#include <stdbool.h>
#include "macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

void ClearScreen(void) {
    HANDLE hConsole;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD cellsWritten;
    DWORD consoleSize;
    COORD home = {0, 0};

    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return;

    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;

    consoleSize = csbi.dwSize.X * csbi.dwSize.Y;

    FillConsoleOutputCharacter(hConsole, (TCHAR)' ', consoleSize, home, &cellsWritten);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, consoleSize, home, &cellsWritten);
    SetConsoleCursorPosition(hConsole, home);
}

int is_help_flag_present(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "/?") == 0) return 1;
    }
    return 0;
}

typedef int (*command_handler_t)(int argc, char **argv);

int cmd_exit(int argc, char **argv) {
    if (is_help_flag_present(argc, argv)) {
        printf("Run 'help exit' for information.\n");
        return 0;
    }
    int code = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '/') {
            code = atoi(argv[i]);
            break;
        }
    }
    exit(code);
}

int cmd_cls(int argc, char **argv) {
    if (is_help_flag_present(argc, argv)) {
        printf("Run 'help cls' for information.\n");
        return 0;
    }
    ClearScreen();
    return 0;
}

int cmd_ver(int argc, char **argv) {
    if (is_help_flag_present(argc, argv)) {
        printf("Run 'help ver' for information.\n");
        return 0;
    }
    printf("Microsoft Windows [Version 10.0.26220.1337]\n");
    return 0;
}

int cmd_cd(int argc, char **argv) {
    if (is_help_flag_present(argc, argv)) {
        printf("Run 'help cd' for information.\n");
        return 0;
    }

    TCHAR currentDir[MAX_PATH] = { 0 };

    if (!GetCurrentDirectory(MAX_PATH, currentDir)) {
        return 1;
    }

    if (argc == 1) {
        printf("%s\n", currentDir);
    }

    const char *targetDir = ".";
    bool switch_drive = false;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '/') {
            if (toupper(argv[i][1]) == 'D') {
                switch_drive = true;
            }
            break;
        } 
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '/') {
            targetDir = argv[i];
            break;
        } 
    }

    if (toupper(currentDir[0]) == toupper(targetDir[0])) {
        if (SetCurrentDirectory(targetDir)) {
            return 0;
        } else {
            fprintf(stderr, "The system could not find the path specified.\n");
            return 1;
        }
    } else {
        if (switch_drive == true) {
            if (SetCurrentDirectory(targetDir)) {
                return 0;
            } else {
                fprintf(stderr, "The system could not find the path specified.\n");
                return 1;
            }
        }
        
        return 0;
    }
}

int cmd_help(int argc, char **argv);

typedef struct {
    const char *name;
    command_handler_t handler;
} Command;

Command commands[] = {
    {"exit", cmd_exit},
    {"cls", cmd_cls},
    {"ver", cmd_ver},
    {"help", cmd_help},
    {"cd", cmd_cd},
    {NULL, NULL}
};

typedef struct {
    const char *msg;
    const char *cmd;
} Help;

Help help_msgs[] = {
    {"Exits the program CMD.EXE or the current batch file.\n\nEXIT [code]\n\ncode: specifies an exit code\n", "exit"},
    {NULL, NULL}
};

int cmd_help(int argc, char **argv) {
    if (argc == 1 || is_help_flag_present(argc, argv)) {
        printf("Available commands:\n\nhelp\nver\ncls\nexit\n\nType help <command> for details.\n");
        return 0;
    }
    char *sub = argv[1];
    for (int i = 0; help_msgs[i].cmd; i++) {
        if (strcmp(sub, help_msgs[i].cmd) == 0) {
            printf(help_msgs[i].msg);
            return 0;
        }
    }
    printf("No help available for: %s\n", sub);
    return 0;
}

int run_command(char *cmd, int mode) {
    if (!cmd) return -1;
    char *cmd_copy = strdup(cmd);
    if (!cmd_copy) return -1;

    while (*cmd_copy == ' ' || *cmd_copy == '\t' || *cmd_copy == '\n') cmd_copy++;

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

    for (int i = 0; commands[i].name; i++) {
        if (strcmp(args[0], commands[i].name) == 0) {
            int ret = commands[i].handler(argc, args);
            free(cmd_copy);
            return ret;
        }
    }

    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    if (!CreateProcess(NULL, cmd_copy, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "'%s' is not recognized as an internal or external command.\n", args[0]);
        free(cmd_copy);
        return 9009;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    free(cmd_copy);
    return (int)exit_code;
}
