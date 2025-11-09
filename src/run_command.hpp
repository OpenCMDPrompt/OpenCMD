#pragma once

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <windows.h>
#include "macros.hpp"

// Forward declarations
char* trimString(char* str);

using command_handler_t = int(*)(int argc, char** argv);

// Clear the console screen
void ClearScreen() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;

    DWORD consoleSize = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD cellsWritten;
    COORD home{0, 0};

    FillConsoleOutputCharacter(hConsole, ' ', consoleSize, home, &cellsWritten);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, consoleSize, home, &cellsWritten);
    SetConsoleCursorPosition(hConsole, home);
}

// Check for "/?" help flag
bool is_help_flag_present(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "/?") == 0) return true;
    }
    return false;
}

// Command implementations
int cmd_exit(int argc, char** argv) {
    if (is_help_flag_present(argc, argv)) {
        std::cout << "Run 'help exit' for information.\n";
        return 0;
    }

    int code = 0;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '/') {
            code = std::atoi(argv[i]);
            break;
        }
    }
    std::exit(code);
}

int cmd_cls(int, char**) {
    ClearScreen();
    return 0;
}

int cmd_ver(int argc, char**) {
    if (is_help_flag_present(argc, nullptr)) {
        std::cout << "Run 'help ver' for information.\n";
        return 0;
    }

    DWORD major = 0, minor = 0, build = 0, ubr = 0;

    // Get major, minor, build from RtlGetVersion
    HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
        RtlGetVersionPtr fn = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
        if (fn != nullptr) {
            RTL_OSVERSIONINFOW rovi = {0};
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (fn(&rovi) == 0) {
                major = rovi.dwMajorVersion;
                minor = rovi.dwMinorVersion;
                build = rovi.dwBuildNumber;
            }
        }
    }

    // Get UBR (update build revision) from registry for the fourth part
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD ubrSize = sizeof(DWORD);
        RegGetValueW(hKey, nullptr, L"UBR", RRF_RT_REG_DWORD, nullptr, &ubr, &ubrSize);
        RegCloseKey(hKey);
    }

    std::cout << "Microsoft Windows [Version "
              << major << "." << minor << "." << build << "." << ubr << "]\n";

    return 0;
}

// Drive directory storage
static std::string drive_dirs[26];
static bool drive_dirs_initialized = false;

static void init_drive_dirs() {
    if (drive_dirs_initialized) return;
    for (auto& s : drive_dirs) s.clear();
    drive_dirs_initialized = true;
}

void set_drive_dir(char drive, const char* path) {
    init_drive_dirs();
    if (!std::isalpha(drive) || !path) return;
    int idx = std::toupper(drive) - 'A';
    if (idx < 0 || idx >= 26) return;
    drive_dirs[idx] = path;
}

const char* get_drive_dir(char drive) {
    init_drive_dirs();
    if (!std::isalpha(drive)) return nullptr;
    int idx = std::toupper(drive) - 'A';
    if (idx < 0 || idx >= 26) return nullptr;
    return drive_dirs[idx].empty() ? nullptr : drive_dirs[idx].c_str();
}

int cmd_cd(int argc, char** argv) {
    if (is_help_flag_present(argc, argv)) {
        std::cout << "Run 'help cd' for information.\n";
        return 0;
    }

    char currentDir[MAX_PATH]{0};
    if (!GetCurrentDirectoryA(MAX_PATH, currentDir)) return 1;

    if (argc == 1) {
        std::cout << currentDir << "\n";
        return 0;
    }

    bool switch_drive = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '/' && std::toupper(argv[i][1]) == 'D') {
            switch_drive = true;
            break;
        }
    }

    const char* arg = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '/') { arg = argv[i]; break; }
    }
    if (!arg) { std::cout << currentDir << "\n"; return 0; }

    std::string target = trimString(strdup(arg));

    bool has_drive = target.size() >= 2 && std::isalpha(target[0]) && target[1] == ':';
    char cur_drive = std::toupper(currentDir[0]);
    char prev_drive = cur_drive;
    std::string saved_prev_dir;

    if (cur_drive >= 'A' && cur_drive <= 'Z') {
        const char* p = get_drive_dir(cur_drive);
        saved_prev_dir = p ? p : currentDir;
    }

    set_drive_dir(cur_drive, currentDir);

    if (!has_drive) {
        if (!SetCurrentDirectoryA(target.c_str())) {
            std::cerr << "The system could not find the path specified.\n";
            return 1;
        }
        char newDir[MAX_PATH]{0};
        if (GetCurrentDirectoryA(MAX_PATH, newDir)) set_drive_dir(cur_drive, newDir);
        return 0;
    }

    // Drive-specific handling
    char target_drive = std::toupper(target[0]);
    const char* after_colon = target.c_str() + 2;

    if (*after_colon == '\0') {
        if (switch_drive) {
            const char* saved = get_drive_dir(target_drive);
            std::string buf = saved ? saved : std::string(1, target_drive) + ":\\";
            if (!SetCurrentDirectoryA(buf.c_str())) {
                std::cerr << "The system could not find the path specified.\n";
                return 1;
            }
            char newDir[MAX_PATH]{0};
            if (GetCurrentDirectoryA(MAX_PATH, newDir)) set_drive_dir(target_drive, newDir);
            return 0;
        } else {
            const char* saved = get_drive_dir(target_drive);
            std::cout << (saved ? saved : (std::string(1, target_drive) + ":\\")) << "\n";
            return 0;
        }
    }

    if (*after_colon == '\\' || *after_colon == '/') {
        if (!SetCurrentDirectoryA(target.c_str())) {
            std::cerr << "The system could not find the path specified.\n";
            return 1;
        }
        char newDir[MAX_PATH]{0};
        if (GetCurrentDirectoryA(MAX_PATH, newDir)) set_drive_dir(target_drive, newDir);

        if (!switch_drive && target_drive != prev_drive) {
            if (!saved_prev_dir.empty()) SetCurrentDirectoryA(saved_prev_dir.c_str());
            else {
                char root[4]{ prev_drive, ':', '\\', 0 };
                SetCurrentDirectoryA(root);
            }
            return 0;
        }
        return 0;
    }

    std::cerr << "Invalid path syntax.\n";
    return 1;
}

// Help system
struct Command {
    const char* name;
    command_handler_t handler;
};

struct Help {
    const char* msg;
    const char* cmd;
};

int cmd_help(int argc, char** argv);

Command commands[] = {
    {"exit", cmd_exit},
    {"cls", cmd_cls},
    {"ver", cmd_ver},
    {"help", cmd_help},
    {"cd", cmd_cd},
    {nullptr, nullptr}
};

Help help_msgs[] = {
    {"Exits the program CMD.EXE or the current batch file.\n\nEXIT [code]\n\ncode: specifies an exit code\n", "exit"},
    {nullptr, nullptr}
};

int cmd_help(int argc, char** argv) {
    if (argc == 1 || is_help_flag_present(argc, argv)) {
        std::cout << "Available commands:\n\nhelp\nver\ncls\nexit\n\nType help <command> for details.\n";
        return 0;
    }
    const char* sub = argv[1];
    for (auto& h : help_msgs) {
        if (!h.cmd) break;
        if (std::strcmp(sub, h.cmd) == 0) {
            std::cout << h.msg;
            return 0;
        }
    }
    std::cout << "No help available for: " << sub << "\n";
    return 0;
}

// Trim string
char* trimString(char* str) {
    char* s = str;
    int start = 0, end = static_cast<int>(std::strlen(s)) - 1;

    while (std::isspace(static_cast<unsigned char>(s[start]))) start++;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end]))) end--;

    if (start > 0 || end < static_cast<int>(std::strlen(s)) - 1) {
        std::memmove(s, s + start, end - start + 1);
        s[end - start + 1] = '\0';
    }
    return s;
}

// Run command
int run_command(char* cmd, int mode) {
    if (!cmd) return -1;

    if (!drive_dirs_initialized) {
        char cwd[MAX_PATH]{0};
        if (GetCurrentDirectoryA(MAX_PATH, cwd)) set_drive_dir(std::toupper(cwd[0]), cwd);
    }

    std::unique_ptr<char, decltype(&std::free)> cmd_copy(strdup(cmd), &std::free);
    if (!cmd_copy) return -1;

    // Skip leading whitespace
    char* ptr = cmd_copy.get();
    while (*ptr && std::isspace(static_cast<unsigned char>(*ptr))) ptr++;

    // Split into arguments
    std::vector<char*> args;
    char* token;
    char* rest = ptr;

    while ((token = strsep(&rest, " \t\n")) != nullptr) {
        if (*token) args.push_back(token);
        if (args.size() >= 63) break;
    }
    args.push_back(nullptr);

    if (args.empty()) return -1;

    // Check built-in commands
    for (auto& c : commands) {
        if (!c.name) break;
        if (std::strcmp(args[0], c.name) == 0) {
            return c.handler(static_cast<int>(args.size() - 1), args.data());
        }
    }

    // Launch external process
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    if (!CreateProcessA(nullptr, trimString(cmd), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        std::cerr << "'" << args[0] << "' is not recognized as an internal or external command.\n";
        return 9009;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exit_code);
}
