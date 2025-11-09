#pragma once

#include <ctype.h>
#include <stdbool.h>
#include "macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

char *trimString(char *str);

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

static char drive_dirs[26][MAX_PATH];
static bool drive_dirs_initialized = false;

static void init_drive_dirs(void) {
    if (drive_dirs_initialized) return;
    for (int i = 0; i < 26; ++i) drive_dirs[i][0] = '\0';
    drive_dirs_initialized = true;
}

// store path for drive letter (expects ASCII letter)
void set_drive_dir(char drive, const char *path) {
    init_drive_dirs();
    if (!isalpha((unsigned char)drive) || !path) return;
    int idx = toupper((unsigned char)drive) - 'A';
    if (idx < 0 || idx >= 26) return;
    strncpy(drive_dirs[idx], path, MAX_PATH - 1);
    drive_dirs[idx][MAX_PATH - 1] = '\0';
}

// return stored path or NULL
const char *get_drive_dir(char drive) {
    init_drive_dirs();
    if (!isalpha((unsigned char)drive)) return NULL;
    int idx = toupper((unsigned char)drive) - 'A';
    if (idx < 0 || idx >= 26) return NULL;
    return drive_dirs[idx][0] ? drive_dirs[idx] : NULL;
}

int cmd_cd(int argc, char **argv) {
    if (is_help_flag_present(argc, argv)) {
        printf("Run 'help cd' for information.\n");
        return 0;
    }

    TCHAR currentDir[MAX_PATH] = {0};
    if (!GetCurrentDirectory(MAX_PATH, currentDir)) {
        return 1;
    }

    if (argc == 1) {
        printf("%s\n", currentDir);
        return 0;
    }

    bool switch_drive = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '/' && toupper((unsigned char)argv[i][1]) == 'D') {
            switch_drive = true;
            break;
        }
    }

    // find first non-flag argument
    const char *arg = NULL;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '/') { arg = argv[i]; break; }
    }
    if (!arg) { printf("%s\n", currentDir); return 0; }

    char *arg_copy = strdup(arg);
    if (!arg_copy) return 1;
    char *target = trimString(arg_copy);

    // determine whether it has drive letter
    bool has_drive = (strlen(target) >= 2) && isalpha((unsigned char)target[0]) && target[1] == ':';
    char cur_drive = toupper((unsigned char)currentDir[0]);
    char prev_drive = cur_drive;
    char saved_prev_dir[MAX_PATH] = {0};
    if (cur_drive >= 'A' && cur_drive <= 'Z') {
        const char *p = get_drive_dir(cur_drive);
        if (p) strncpy(saved_prev_dir, p, MAX_PATH-1);
        else strncpy(saved_prev_dir, currentDir, MAX_PATH-1);
    }

    set_drive_dir(cur_drive, currentDir);

    if (!has_drive) {
        if (!SetCurrentDirectory(target)) {
            fprintf(stderr, "The system could not find the path specified.\n");
            free(arg_copy);
            return 1;
        }
        // Update stored dir for current drive
        TCHAR newDir[MAX_PATH] = {0};
        if (GetCurrentDirectory(MAX_PATH, newDir)) set_drive_dir(cur_drive, newDir);
        free(arg_copy);
        return 0;
    }

    // We have a drive letter
    char target_drive = toupper((unsigned char)target[0]);
    const char *after_colon = target + 2; // points after "X:"

    if (*after_colon == '\0') {
        if (switch_drive) {
            // switch to that drive and restore its stored dir or root
            const char *saved = get_drive_dir(target_drive);
            char buf[MAX_PATH];
            if (saved && *saved) {
                strncpy(buf, saved, MAX_PATH-1);
                buf[MAX_PATH-1] = '\0';
            } else {
                buf[0] = target_drive; buf[1] = ':'; buf[2] = '\\'; buf[3] = '\0';
            }
            if (!SetCurrentDirectory(buf)) {
                fprintf(stderr, "The system could not find the path specified.\n");
                free(arg_copy);
                return 1;
            }
            // store new current for that drive
            TCHAR newDir[MAX_PATH] = {0};
            if (GetCurrentDirectory(MAX_PATH, newDir)) set_drive_dir(target_drive, newDir);
            // print nothing — caller prompt will reflect new drive/dir
            free(arg_copy);
            return 0;
        } else {
            const char *saved = get_drive_dir(target_drive);
            if (saved && *saved) printf("%s\n", saved);
            else printf("%c:\\\n", target_drive);
            free(arg_copy);
            return 0;
        }
    }

    if (*after_colon == '\\' || *after_colon == '/') {
        if (!SetCurrentDirectory(target)) {
            fprintf(stderr, "The system could not find the path specified.\n");
            free(arg_copy);
            return 1;
        }
        TCHAR newDir[MAX_PATH] = {0};
        if (GetCurrentDirectory(MAX_PATH, newDir)) set_drive_dir(target_drive, newDir);

        if (!switch_drive && target_drive != prev_drive) {
            if (saved_prev_dir[0]) {
                SetCurrentDirectory(saved_prev_dir);
            } else {
                char root[4] = { prev_drive, ':', '\\', 0 };
                SetCurrentDirectory(root);
            }
            // active drive remains unchanged; per-drive stored dir updated for target_drive
            free(arg_copy);
            return 0;
        }

        free(arg_copy);
        return 0;
    }

    fprintf(stderr, "Invalid path syntax.\n");
    free(arg_copy);
    return 1;
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

char *trimString(char *str_untrimmed) {
    char *str = str_untrimmed;
    int start = 0, end = strlen(str) - 1;
  
    while (isspace((unsigned char)str[start])) start++;
    while (end > start && isspace((unsigned char)str[end])) end--;
  
    // If the string was trimmed, adjust the null terminator
    if (start > 0 || end < (strlen(str) - 1)) {
        memmove(str, str + start, end - start + 1);
        str[end - start + 1] = '\0';
    }
    return str;
}

int run_command(char *cmd, int mode) {
    if (!cmd) return -1;
    if (!drive_dirs_initialized) {
        TCHAR cwd[MAX_PATH];
        if (GetCurrentDirectory(MAX_PATH, cwd)) {
            set_drive_dir(toupper(cwd[0]), cwd);
        }
    }
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
    char *cmd_trimmed = trimString(cmd);
    if (!CreateProcess(NULL, cmd_trimmed, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
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
