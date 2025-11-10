#pragma once

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <cwctype>
#include <iostream>
#include <windows.h>
#include "macros.hpp"

extern bool echo_enabled;

char* trimString(char* str);
using command_handler_t = int(*)(int argc, char** argv);

namespace cmd {

enum class TokenKind {
    Identifier,
    String,
    Flag,
    End,
};

struct Token {
    TokenKind kind;
    std::string text;
};

class Tokenizer {
private:
    const char* p;
public:
    explicit Tokenizer(const char* s) : p(s ? s : "") {}

    void skip_ws() {
        while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p;
    }

    std::string parse_quoted(char quote) {
        std::string out;
        ++p; // skip opening quote
        while (*p) {
            if (*p == quote) { ++p; break; }
            if (*p == '\\') {
                const char* next = p + 1;
                if (*next == quote || *next == '\\') {
                    ++p; // skip backslash
                    out.push_back(*p); // add escaped char
                } else {
                    out.push_back(*p); // keep the backslash as-is
                }
                ++p;
            } else {
                out.push_back(*p);
                ++p;
            }
        }
        return out;
    }

    Token next() {
        skip_ws();
        if (!*p) return {TokenKind::End, std::string()};

        if (*p == '/') {
            const char* start = p;
            ++p;
            while (*p && !std::isspace(static_cast<unsigned char>(*p))) ++p;
            return {TokenKind::Flag, std::string(start, static_cast<size_t>(p - start))};
        }

        if (*p == '"' || *p == '\'') {
            char q = *p;
            std::string s = parse_quoted(q);
            return {TokenKind::String, s};
        }

        const char* start = p;
        while (*p && !std::isspace(static_cast<unsigned char>(*p))) ++p;
        return {TokenKind::Identifier, std::string(start, static_cast<size_t>(p - start))};
    }

    std::vector<Token> tokenize() {
        std::vector<Token> out;
        while (true) {
            Token t = next();
            if (t.kind == TokenKind::End) break;
            out.push_back(std::move(t));
            if (out.size() >= 256) break;
        }
        return out;
    }
};

struct Arg {
    bool is_flag;
    std::string text;
};

struct CommandAST {
    std::string name;
    std::vector<Arg> args;
};

class Parser {
private:
    const std::vector<Token>& toks;
    size_t pos;
public:
    explicit Parser(const std::vector<Token>& t) : toks(t), pos(0) {}

    bool empty() const { return pos >= toks.size(); }

    CommandAST parse() {
        CommandAST cmd;
        if (toks.empty()) return cmd;
        cmd.name = toks[0].text;
        for (size_t i = 1; i < toks.size(); ++i) {
            const Token& tk = toks[i];
            Arg a;
            a.is_flag = (tk.kind == TokenKind::Flag);
            a.text = tk.text;
            cmd.args.push_back(std::move(a));
        }
        return cmd;
    }
};

}

std::string canonicalize(const std::string& path) {
    char buffer[MAX_PATH];
    DWORD result = GetFullPathNameA(path.c_str(), MAX_PATH, buffer, nullptr);
    if (result == 0 || result > MAX_PATH) {
        throw std::runtime_error("Failed to canonicalize path");
    }
    return std::string(buffer);
}

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

bool is_help_flag_present(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "/?") == 0) return true;
    }
    return false;
}

int cmd_exit(int argc, char** argv) {
    if (is_help_flag_present(argc, argv)) {
        std::cout << "Run 'help exit' for information." << "\n";
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

int cmd_echo(int argc, char** argv) {
    if (is_help_flag_present(argc, argv)) {
        std::cout << "Run 'help echo' for information." << "\n";
        return 0;
    }

    if (argc == 1) {
        std::cout << "ECHO is " << (echo_enabled ? "on" : "off") << "\n";
        return 0;
    }

    std::string message;
    for (int i = 1; i < argc; ++i) {
        message += argv[i];
        if (i < argc - 1) message += " ";
    }

    if (message == "on") {
        echo_enabled = true;
    } else if (message == "off") {
        echo_enabled = false;
    } else if (message == ".") {
        std::cout << "\n";
    } else {
        std::cout << message << "\n";
    }

    return 0;
}

int cmd_ver(int argc, char** argv) {
    if (is_help_flag_present(argc, argv)) {
        std::cout << "Run 'help ver' for information." << "\n";
        return 0;
    }

    DWORD major = 0, minor = 0, build = 0, ubr = 0;

    HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
        RtlGetVersionPtr fn = reinterpret_cast<RtlGetVersionPtr>(::GetProcAddress(hMod, "RtlGetVersion"));
        if (fn != nullptr) {
            RTL_OSVERSIONINFOW rovi{};  // zero-initialize all members
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (fn(&rovi) == 0) {
                major = rovi.dwMajorVersion;
                minor = rovi.dwMinorVersion;
                build = rovi.dwBuildNumber;
            }
        }
    }

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD ubrSize = sizeof(DWORD);
        RegGetValueW(hKey, nullptr, L"UBR", RRF_RT_REG_DWORD, nullptr, &ubr, &ubrSize);
        RegCloseKey(hKey);
    }

    std::cout << "Microsoft Windows [Version "
              << major << "." << minor << "." << build << "." << ubr << "]" << "\n";

    return 0;
}

static std::string drive_dirs[26];
static bool drive_dirs_initialized = false;

static void init_drive_dirs() {
    if (drive_dirs_initialized) return;
    for (auto& s : drive_dirs) s.clear();
    drive_dirs_initialized = true;
}

void set_drive_dir(char drive, const char* path) {
    init_drive_dirs();
    if (!std::isalpha(static_cast<unsigned char>(drive)) || !path) return;
    int idx = std::toupper(static_cast<unsigned char>(drive)) - 'A';
    if (idx < 0 || idx >= 26) return;
    drive_dirs[idx] = path;
}

const char* get_drive_dir(char drive) {
    init_drive_dirs();
    if (!std::isalpha(static_cast<unsigned char>(drive))) return nullptr;
    int idx = std::toupper(static_cast<unsigned char>(drive)) - 'A';
    if (idx < 0 || idx >= 26) return nullptr;
    return drive_dirs[idx].empty() ? nullptr : drive_dirs[idx].c_str();
}

std::string strip_quotes(const std::string& s) {
    if (s.size() >= 2 && ((s.front() == '\"' && s.back() == '\"') || 
                          (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

int cmd_cd(int argc, char** argv) {
    if (is_help_flag_present(argc, argv)) {
        std::cout << "Run 'help cd' for information." << "\n";
        return 0;
    }

    char currentDir[MAX_PATH]{0};
    if (!GetCurrentDirectoryA(MAX_PATH, currentDir)) return 1;
    if (!SetCurrentDirectoryA(currentDir)) {
        std::cerr << "The system could not find the path specified." << "\n";
        return 1;
    }


    if (argc == 1) {
        std::cout << currentDir << "\n";
        return 0;
    }

    bool switch_drive = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '/' && std::toupper(static_cast<unsigned char>(argv[i][1])) == 'D') {
            switch_drive = true;
            break;
        }
    }

    const char* arg = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '/') { arg = argv[i]; break; }
    }
    if (!arg) { std::cout << currentDir << "\n"; return 0; }

    std::unique_ptr<char, decltype(&std::free)> argcopy(strdup(arg), &std::free);
    std::string target = trimString(argcopy.get());
    //target = strip_quotes(target);

    bool has_drive = target.size() >= 2 && std::isalpha(static_cast<unsigned char>(target[0])) && target[1] == ':';
    char cur_drive = std::toupper(static_cast<unsigned char>(currentDir[0]));
    char prev_drive = cur_drive;
    std::string saved_prev_dir;

    if (cur_drive >= 'A' && cur_drive <= 'Z') {
        const char* p = get_drive_dir(cur_drive);
        saved_prev_dir = p ? p : currentDir;
    }

    set_drive_dir(cur_drive, currentDir);

    if (!has_drive) {
        if (target.at(0) == '\\' || target.at(0) == '/') {
            std::string cur_drive_full_with_colon = "a";
            cur_drive_full_with_colon = cur_drive;
            std::string targetAbs = cur_drive_full_with_colon + ":\\" + target;
            if (!SetCurrentDirectoryA(targetAbs.c_str())) {
                std::cerr << "The system could not find the path specified." << "\n";
                return 1;
            }
            return 0;
        }

        std::string currentDir2 = currentDir;

        std::string target2 = target;

        std::string targetAbs = currentDir2 + "\\" + target2;
        targetAbs = canonicalize(targetAbs);
 
        if (!SetCurrentDirectoryA(targetAbs.c_str())) {
            DWORD errorCode = GetLastError();
            std::cout << "Failed to open file. Error code: " << errorCode << "\n";
            std::cout << "SetCurrentDirectoryA(" << targetAbs << ")\n";
            std::cerr << "The system could not find the path specified." << "\n";
            return 1;
        }
        char newDir[MAX_PATH]{0};
        if (GetCurrentDirectoryA(MAX_PATH, newDir)) set_drive_dir(cur_drive, newDir);
        return 0;
    }

    char target_drive = std::toupper(static_cast<unsigned char>(target[0]));
    const char* after_colon = target.c_str() + 2;

    if (*after_colon == '\0') {
        if (switch_drive) {
            const char* saved = get_drive_dir(target_drive);
            std::string buf = saved ? saved : std::string(1, target_drive) + ":\\";
            if (!SetCurrentDirectoryA(buf.c_str())) {
                std::cerr << "The system could not find the path specified." << "\n";
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
            std::cerr << "The system could not find the path specified." << "\n";
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

    std::cerr << "Invalid path syntax." << "\n";
    return 1;
}

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
    {"echo", cmd_echo},
    {nullptr, nullptr}
};

Help help_msgs[] = {
    {"Exits the program CMD.EXE or the current batch file.\n\nEXIT [code]\n\ncode: specifies an exit code\n", "exit"},
    {"Clears the screen and moves the cursor to the top-left corner.\n\nCLS\n", "cls"},
    {"Displays the current Windows version.\n\nVER\n", "ver"},
    {"Changes the current directory.\n\nCD [path] [/D]\n\n/path: optional, specifies the directory to change to.\n/D: switches the current drive in addition to changing the directory.\nIf no path is provided, displays the current directory.\nSupports drive switching and remembers last directories per drive.\n", "cd"},
    {"Displays messages, turns command echoing on or off.\n\nECHO [on | off | message | .]\n\non:  enables echoing of commands\noff: disables echoing of commands\nmessage: prints the specified message\n.: prints a blank line\nIf no arguments are provided, displays the current echo state.\n", "echo"},
    {"Displays this help information.\n\nHELP [command]\n\nIf no command is provided, lists all available commands.\nUse 'HELP <command>' for detailed information about a specific command.\n", "help"},
    {nullptr, nullptr}
};

int cmd_help(int argc, char** argv) {
    if (argc == 1 || is_help_flag_present(argc, argv)) {
        std::cout << "Available commands:" << "\n\n";
        std::cout << "help\nver\ncls\nexit\ncd\necho\n\n";
        std::cout << "Type help <command> for details." << "\n";
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

int run_command(char* cmdline, int /*mode*/) {
    if (!cmdline) return -1;

    if (!drive_dirs_initialized) {
        char cwd[MAX_PATH]{0};
        if (GetCurrentDirectoryA(MAX_PATH, cwd)) set_drive_dir(std::toupper(static_cast<unsigned char>(cwd[0])), cwd);
    }

    cmd::Tokenizer tok(cmdline);
    std::vector<cmd::Token> tokens = tok.tokenize();
    if (tokens.empty()) return -1;

    cmd::Parser parser(tokens);
    cmd::CommandAST ast = parser.parse();
    if (ast.name.empty()) return -1;

    std::vector<std::unique_ptr<char, decltype(&std::free)>> owned_strings;
    std::vector<char*> argv;

    {
        char* s = strdup(ast.name.c_str());
        owned_strings.emplace_back(s, &std::free);
        argv.push_back(s);
    }

    for (auto& a : ast.args) {
        char* s = strdup(a.text.c_str());
        owned_strings.emplace_back(s, &std::free);
        argv.push_back(s);
    }
    argv.push_back(nullptr);

    for (auto& c : commands) {
        if (!c.name) break;
        if (std::strcmp(argv[0], c.name) == 0) {
            return c.handler(static_cast<int>(argv.size() - 1), argv.data());
        }
    }

    std::unique_ptr<char, decltype(&std::free)> cmd_copy(strdup(cmdline), &std::free);
    if (!cmd_copy) return -1;
    trimString(cmd_copy.get());

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    if (!CreateProcessA(nullptr, cmd_copy.get(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        std::cerr << "'" << argv[0] << "' is not recognized as an internal or external command." << "\n";
        return 9009;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exit_code);
}
