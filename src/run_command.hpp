#pragma once

#include "macros.hpp"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

extern bool echo_enabled;

char *trimString(char *str);
using command_handler_t = int (*)(int argc, char **argv);

namespace cmd {

enum class TokenKind {
    Identifier,
    String,
    Flag,
    End,
    Symbol,
};

struct Token {
    TokenKind kind;
    std::string text;
};

class Tokenizer {
  private:
    const char *p;

    static bool is_ws(char c) { return std::isspace(static_cast<unsigned char>(c)); }

    static bool is_flag_start(char c) { return c == '/'; }

    void skip_ws() {
        while (*p && is_ws(*p))
            ++p;
    }

    std::string parse_quoted(char quote) {
        std::string out;
        ++p;
        while (*p) {
            if (*p == quote) {
                ++p;
                break;
            }
            if (*p == '\\' && (*(p + 1) == quote || *(p + 1) == '\\')) {
                ++p;
            }
            out.push_back(*p);
            ++p;
        }
        return out;
    }

  public:
    explicit Tokenizer(const char *s) : p(s ? s : "") {}

    Token next() {
        skip_ws();
        if (!*p)
            return {TokenKind::End, ""};

        if (*p == '"' || *p == '\'') {
            char q = *p;
            std::string s = parse_quoted(q);
            return {TokenKind::String, s};
        }

        if (is_flag_start(*p)) {
            const char *start = p++;
            while (*p && !is_ws(*p))
                ++p;
            return {TokenKind::Flag, std::string(start, static_cast<size_t>(p - start))};
        }

        const char *start = p;
        while (*p && !is_ws(*p))
            ++p;

        std::string text(start, static_cast<size_t>(p - start));

        if (text == "echo." || text == "echo") {
            return {TokenKind::Identifier, text};
        }

        if (text.rfind("echo.", 0) == 0 && text.size() > 5) {
            p = start + 4;
            return {TokenKind::Identifier, "echo"};
        }

        return {TokenKind::Identifier, text};
    }

    std::vector<Token> tokenize() {
        std::vector<Token> out;
        while (true) {
            Token t = next();
            if (t.kind == TokenKind::End)
                break;
            out.push_back(std::move(t));
            if (out.size() >= 256)
                break;
        }

        for (size_t i = 0; i + 1 < out.size(); ++i) {
            if (out[i].text == "echo" && out[i + 1].text.rfind(".", 0) == 0 &&
                out[i + 1].text.size() > 1) {
                out[i + 1].text = out[i + 1].text.substr(1);
            }
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
    const std::vector<Token> &toks;
    size_t pos;

  public:
    explicit Parser(const std::vector<Token> &t) : toks(t), pos(0) {}
    bool empty() const { return pos >= toks.size(); }
    CommandAST parse() {
        CommandAST cmd;
        if (toks.empty())
            return cmd;

        cmd.name = toks[0].text;

        if (cmd.name == "echo.") {
            cmd.name = "echo";
            Arg a;
            a.text = ".";
            cmd.args.push_back(a);
            return cmd;
        }

        if (cmd.name.rfind("echo.", 0) == 0 && cmd.name.size() > 5) {
            std::string rest = cmd.name.substr(5);
            cmd.name = "echo";
            Arg a;
            a.text = rest;
            cmd.args.push_back(a);
            return cmd;
        }

        for (size_t i = 1; i < toks.size(); ++i) {
            const Token &tk = toks[i];
            Arg a;
            a.is_flag = (tk.kind == TokenKind::Flag);
            a.text = tk.text;
            cmd.args.push_back(std::move(a));
        }

        return cmd;
    }
};

} // namespace cmd

std::string canonicalize(const std::string &path) {
    namespace fs = std::filesystem;
    extern const char *get_drive_dir(char);
    std::string p = path;
    for (char &c : p)
        if (c == '/')
            c = '\\';
    if (p.empty()) {
        try {
            return fs::absolute(fs::path(".")).lexically_normal().string();
        } catch (...) {
            return std::string();
        }
    }
    if (p.size() >= 2 && p[0] == '\\' && p[1] == '\\') {
        try {
            return fs::path(p).lexically_normal().string();
        } catch (...) {
            return p;
        }
    }
    if (p.size() >= 2 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':') {
        char drive = static_cast<char>(std::toupper(static_cast<unsigned char>(p[0])));
        if (p.size() == 2) {
            const char *saved = get_drive_dir(drive);
            if (saved && saved[0])
                return std::string(saved);
            return std::string(1, drive) + ":\\";
        }
        if (p.size() >= 3 && (p[2] == '\\')) {
            try {
                return fs::path(p).lexically_normal().string();
            } catch (...) {
                return p;
            }
        } else {
            const char *saved = get_drive_dir(drive);
            std::string base =
                (saved && saved[0]) ? std::string(saved) : (std::string(1, drive) + ":\\");
            std::string remainder = p.substr(2);
            std::string combined = base;
            if (!combined.empty() && combined.back() != '\\')
                combined += '\\';
            combined += remainder;
            try {
                return fs::path(combined).lexically_normal().string();
            } catch (...) {
                return combined;
            }
        }
    }
    if (p.size() >= 1 && p[0] == '\\') {
        std::string cur;
        try {
            cur = fs::current_path().string();
        } catch (...) {
            cur = "C:\\";
        }
        char drive =
            cur.empty() ? 'C' : static_cast<char>(std::toupper(static_cast<unsigned char>(cur[0])));
        std::string combined = std::string(1, drive) + ":" + p;
        try {
            return fs::path(combined).lexically_normal().string();
        } catch (...) {
            return combined;
        }
    }
    try {
        fs::path ap = fs::absolute(fs::path(p));
        return ap.lexically_normal().string();
    } catch (...) {
        return p;
    }
}

void ClearScreen() { std::cout << "\033[2J\033[3J\033[H"; }

bool is_help_flag_present(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "/?") == 0)
            return true;
    }
    return false;
}

int cmd_exit(int argc, char **argv) {
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

int cmd_cls(int, char **) {
    ClearScreen();
    return 0;
}

int cmd_echo(int argc, char **argv) {
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
        if (i < argc - 1)
            message += " ";
    }
    if (message == "on")
        echo_enabled = true;
    else if (message == "off")
        echo_enabled = false;
    else if (message == ".")
        std::cout << "\n";
    else
        std::cout << message << "\n";
    return 0;
}

void print_drive_info(const std::string &path) {
    char root[MAX_PATH]{0};
    // Extract drive root, e.g., "C:\\"
    if (path.size() >= 2 && path[1] == ':') {
        std::snprintf(root, sizeof(root), "%c:\\", path[0]);
    } else {
        GetFullPathNameA(path.c_str(), MAX_PATH, root, nullptr);
        root[3] = '\0'; // keep only drive root
    }

    char volumeName[MAX_PATH]{0};
    DWORD serialNumber = 0;
    if (GetVolumeInformationA(root, volumeName, sizeof(volumeName), &serialNumber, nullptr, nullptr,
                              nullptr, 0)) {
        std::cout << " Volume in drive " << (char)std::toupper(root[0]) << " is ";
        if (volumeName[0])
            std::cout << volumeName << "\n";
        else
            std::cout << "has no label.\n";

        std::cout << " Volume Serial Number is " << std::uppercase << std::hex
                  << ((serialNumber >> 16) & 0xFFFF) << "-" << (serialNumber & 0xFFFF) << std::dec
                  << "\n\n";
    } else {
        std::cerr << "Unable to retrieve volume info.\n";
    }
}

int cmd_dir(int argc, char **argv) {
    if (is_help_flag_present(argc, argv)) {
        std::cout << "Run 'help cd' for information." << "\n";
        return 0;
    }

    namespace fs = std::filesystem;
    std::string target = (argc > 1) ? argv[1] : ".";
    print_drive_info(target);
    std::cout << " Directory of " << canonicalize(target) << "\n\n";

    uintmax_t total_size = 0;
    size_t file_count = 0, dir_count = 0;

    auto print_entry = [&](const fs::path &path, bool is_dir) {
        auto ftime = fs::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - decltype(ftime)::clock::now() + std::chrono::system_clock::now());
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        std::tm tm{};
        localtime_s(&tm, &cftime);

        char timebuf[32];
        std::strftime(timebuf, sizeof(timebuf), "%m-%d-%Y  %I:%M %p", &tm);

        std::cout << timebuf << "  ";
        if (is_dir) {
            std::cout << std::setw(12) << std::left << "<DIR>";
            dir_count++;
        } else {
            auto sz = fs::file_size(path);
            std::cout << std::setw(12) << std::right << sz;
            total_size += sz;
            file_count++;
        }
        std::cout << "  " << path.filename().string() << "\n";
    };

    try {
        print_entry(target, true);
        print_entry(target + "/..", true);

        for (const auto &entry : fs::directory_iterator(target)) {
            print_entry(entry.path(), entry.is_directory());
        }

        std::cout << "              " << file_count << " File(s)    " << total_size << " bytes\n";
        std::cout << "              " << dir_count << " Dir(s)\n";
        return 0;
    } catch (const fs::filesystem_error &) {
        std::cerr << "The system cannot find the path specified.\n";
        return 1;
    }
}

int cmd_ver(int argc, char **argv) {
    if (is_help_flag_present(argc, argv)) {
        std::cout << "Run 'help ver' for information." << "\n";
        return 0;
    }
    DWORD major = 0, minor = 0, build = 0, ubr = 0;
    HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        typedef LONG(WINAPI * RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
        RtlGetVersionPtr fn =
            reinterpret_cast<RtlGetVersionPtr>(::GetProcAddress(hMod, "RtlGetVersion"));
        if (fn != nullptr) {
            RTL_OSVERSIONINFOW rovi{};
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (fn(&rovi) == 0) {
                major = rovi.dwMajorVersion;
                minor = rovi.dwMinorVersion;
                build = rovi.dwBuildNumber;
            }
        }
    }
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                      KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD ubrSize = sizeof(DWORD);
        RegGetValueW(hKey, nullptr, L"UBR", RRF_RT_REG_DWORD, nullptr, &ubr, &ubrSize);
        RegCloseKey(hKey);
    }
    std::cout << "Microsoft Windows [Version " << major << "." << minor << "." << build << "."
              << ubr << "]" << "\n";
    return 0;
}

static std::string drive_dirs[26];
static bool drive_dirs_initialized = false;

static void init_drive_dirs() {
    if (drive_dirs_initialized)
        return;
    for (auto &s : drive_dirs)
        s.clear();
    drive_dirs_initialized = true;
}

void set_drive_dir(char drive, const char *path) {
    init_drive_dirs();
    if (!std::isalpha(static_cast<unsigned char>(drive)) || !path)
        return;
    int idx = std::toupper(static_cast<unsigned char>(drive)) - 'A';
    if (idx < 0 || idx >= 26)
        return;
    drive_dirs[idx] = path;
}

const char *get_drive_dir(char drive) {
    init_drive_dirs();
    if (!std::isalpha(static_cast<unsigned char>(drive)))
        return nullptr;
    int idx = std::toupper(static_cast<unsigned char>(drive)) - 'A';
    if (idx < 0 || idx >= 26)
        return nullptr;
    return drive_dirs[idx].empty() ? nullptr : drive_dirs[idx].c_str();
}

std::string strip_quotes(const std::string &s) {
    if (s.size() >= 2 &&
        ((s.front() == '\"' && s.back() == '\"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

int cmd_cd(int argc, char **argv) {
    if (is_help_flag_present(argc, argv)) {
        std::cout << "Run 'help cd' for information." << "\n";
        return 0;
    }
    std::filesystem::path currentPath;
    try {
        currentPath = std::filesystem::current_path();
    } catch (...) {
        return 1;
    }
    if (argc == 1) {
        std::cout << currentPath.string() << "\n";
        return 0;
    }
    bool switch_drive = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '/' && std::toupper(static_cast<unsigned char>(argv[i][1])) == 'D') {
            switch_drive = true;
            break;
        }
    }
    const char *arg = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '/') {
            arg = argv[i];
            break;
        }
    }
    if (!arg) {
        std::cout << currentPath.string() << "\n";
        return 0;
    }
    std::unique_ptr<char, decltype(&std::free)> argcopy(strdup(arg), &std::free);
    std::string target = trimString(argcopy.get());
    bool has_drive = target.size() >= 2 && std::isalpha(static_cast<unsigned char>(target[0])) &&
                     target[1] == ':';
    char cur_drive = 0;
    {
        auto root_name = currentPath.root_name().string();
        if (!root_name.empty() && std::isalpha(static_cast<unsigned char>(root_name[0])))
            cur_drive = std::toupper(static_cast<unsigned char>(root_name[0]));
    }
    if (cur_drive == 0 && !currentPath.string().empty())
        cur_drive = std::toupper(static_cast<unsigned char>(currentPath.string()[0]));
    char prev_drive = cur_drive;
    std::string saved_prev_dir;
    if (cur_drive >= 'A' && cur_drive <= 'Z') {
        const char *p = get_drive_dir(cur_drive);
        saved_prev_dir = p ? p : currentPath.string();
    }
    set_drive_dir(cur_drive, currentPath.string().c_str());
    try {
        if (!has_drive) {
            if (target == "\\" || target == "/") {
                std::string root = std::string(1, cur_drive) + ":\\";
                try {
                    std::filesystem::current_path(root);
                } catch (...) {
                    std::cerr << "The system could not find the path specified." << "\n";
                    return 1;
                }
                std::filesystem::path newDir = std::filesystem::current_path();
                set_drive_dir(cur_drive, newDir.string().c_str());
                return 0;
            }
            std::filesystem::path joined = currentPath / std::filesystem::path(target);
            std::filesystem::path canon = std::filesystem::weakly_canonical(joined);
            try {
                std::filesystem::current_path(canon);
            } catch (...) {
                std::cerr << "The system could not find the path specified." << "\n";
                return 1;
            }
            std::filesystem::path newDir = std::filesystem::current_path();
            set_drive_dir(cur_drive, newDir.string().c_str());
            return 0;
        }
        char target_drive = std::toupper(static_cast<unsigned char>(target[0]));
        const char *after_colon = target.c_str() + 2;
        if (*after_colon == '\0') {
            if (switch_drive) {
                const char *saved = get_drive_dir(target_drive);
                std::string buf = saved ? saved : std::string(1, target_drive) + ":\\";
                try {
                    std::filesystem::current_path(buf);
                } catch (...) {
                    return 1;
                }
                std::filesystem::path newDir = std::filesystem::current_path();
                set_drive_dir(target_drive, newDir.string().c_str());
                return 0;
            } else {
                const char *saved = get_drive_dir(target_drive);
                std::cout << (saved ? saved : (std::string(1, target_drive) + ":\\")) << "\n";
                return 0;
            }
        }
        if (*after_colon == '\\' || *after_colon == '/') {
            std::filesystem::path p(target);
            std::filesystem::path canon = std::filesystem::weakly_canonical(p);
            try {
                std::filesystem::current_path(canon);
            } catch (...) {
                return 1;
            }
            std::filesystem::path newDir = std::filesystem::current_path();
            set_drive_dir(target_drive, newDir.string().c_str());
            if (!switch_drive && target_drive != prev_drive) {
                if (!saved_prev_dir.empty()) {
                    try {
                        std::filesystem::current_path(saved_prev_dir);
                    } catch (...) {
                        std::filesystem::path root(std::string(1, prev_drive) + ":\\");
                        std::filesystem::current_path(root);
                    }
                } else {
                    std::filesystem::path root(std::string(1, prev_drive) + ":\\");
                    std::filesystem::current_path(root);
                }
                return 0;
            }
            return 0;
        }
        std::cerr << "Invalid path syntax." << "\n";
        return 1;
    } catch (const std::filesystem::filesystem_error &) {
        std::cerr << "The system could not find the path specified." << "\n";
        return 1;
    }
}

struct Command {
    const char *name;
    command_handler_t handler;
};

struct Help {
    const char *msg;
    const char *cmd;
};

int cmd_help(int argc, char **argv);

Command commands[] = {{"exit", cmd_exit}, {"cls", cmd_cls},   {"ver", cmd_ver}, {"help", cmd_help},
                      {"cd", cmd_cd},     {"echo", cmd_echo}, {"dir", cmd_dir}, {nullptr, nullptr}};

Help help_msgs[] = {
    {"Exits the program CMD.EXE or the current batch file.\n\nEXIT [code]\n\ncode: specifies an "
     "exit code\n",
     "exit"},
    {"Clears the screen and moves the cursor to the top-left corner.\n\nCLS\n", "cls"},
    {"Displays the current Windows version.\n\nVER\n", "ver"},
    {"Changes the current directory.\n\nCD [path] [/D]\n\n/path: optional, specifies the directory "
     "to change to.\n/D: switches the current drive in addition to changing the directory.\nIf no "
     "path is provided, displays the current directory.\nSupports drive switching and remembers "
     "last directories per drive.\n",
     "cd"},
    {"Displays messages, turns command echoing on or off.\n\nECHO [on | off | message | .]\n\non:  "
     "enables echoing of commands\noff: disables echoing of commands\nmessage: prints the "
     "specified message\n.: prints a blank line\nIf no arguments are provided, displays the "
     "current echo state.\n",
     "echo"},
    {"Displays this help information.\n\nHELP [command]\n\nIf no command is provided, lists all "
     "available commands.\nUse 'HELP <command>' for detailed information about a specific "
     "command.\n",
     "help"},
    {nullptr, nullptr}};

int cmd_help(int argc, char **argv) {
    if (argc == 1 || is_help_flag_present(argc, argv)) {
        std::cout << "Available commands:\n\nhelp\nver\ncls\nexit\ncd\necho\ndir\n\n";
        std::cout << "Type help <command> for details.\n";
        return 0;
    }
    const char *sub = argv[1];
    for (auto &h : help_msgs) {
        if (!h.cmd)
            break;
        if (std::strcmp(sub, h.cmd) == 0) {
            std::cout << h.msg;
            return 0;
        }
    }
    std::cout << "No help available for: " << sub << "\n";
    return 0;
}

char *trimString(char *str) {
    char *s = str;
    int start = 0, end = static_cast<int>(std::strlen(s)) - 1;
    while (std::isspace(static_cast<unsigned char>(s[start])))
        start++;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end])))
        end--;
    if (start > 0 || end < static_cast<int>(std::strlen(s)) - 1) {
        std::memmove(s, s + start, end - start + 1);
        s[end - start + 1] = '\0';
    }
    return s;
}

int run_command(const char *cmdline, int) {
    if (!cmdline)
        return -1;
    if (!drive_dirs_initialized) {
        char cwd[MAX_PATH]{0};
        if (GetCurrentDirectoryA(MAX_PATH, cwd))
            set_drive_dir(std::toupper(static_cast<unsigned char>(cwd[0])), cwd);
    }
    cmd::Tokenizer tok(cmdline);
    std::vector<cmd::Token> tokens = tok.tokenize();
    if (tokens.empty())
        return -1;
    cmd::Parser parser(tokens);
    cmd::CommandAST ast = parser.parse();
    if (ast.name.empty())
        return -1;
    std::vector<std::unique_ptr<char, decltype(&std::free)>> owned_strings;
    std::vector<char *> argv;
    {
        char *s = strdup(ast.name.c_str());
        owned_strings.emplace_back(s, &std::free);
        argv.push_back(s);
    }
    for (auto &a : ast.args) {
        char *s = strdup(a.text.c_str());
        owned_strings.emplace_back(s, &std::free);
        argv.push_back(s);
    }
    argv.push_back(nullptr);
    for (auto &c : commands) {
        if (!c.name)
            break;
        if (std::strcmp(argv[0], c.name) == 0) {
            return c.handler(static_cast<int>(argv.size() - 1), argv.data());
        }
    }
    std::unique_ptr<char, decltype(&std::free)> cmd_copy(strdup(cmdline), &std::free);
    if (!cmd_copy)
        return -1;
    trimString(cmd_copy.get());
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    if (!CreateProcessA(nullptr, cmd_copy.get(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si,
                        &pi)) {
        std::cerr << "'" << argv[0] << "' is not recognized as an internal or external command.\n";
        return 9009;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exit_code);
}
