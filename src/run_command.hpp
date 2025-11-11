#pragma once

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
  public:
    explicit Tokenizer(const char *s);
    Token next();
    std::vector<Token> tokenize();
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
  public:
    explicit Parser(const std::vector<Token> &t);
    bool empty() const;
    CommandAST parse();
};

} // namespace cmd

std::string canonicalize(const std::string &path);
void ClearScreen();
bool is_help_flag_present(int argc, char **argv);
bool is_flag_present(int argc, char **argv, std::string flag);
int cmd_exit(int argc, char **argv);
int cmd_cls(int, char **);
int cmd_echo(int argc, char **argv);
void print_drive_info(const std::string &path);
int cmd_dir(int argc, char **argv);
int cmd_ver(int argc, char **argv);
int cmd_openver(int argc, char **argv);
void set_drive_dir(char drive, const char *path);
const char *get_drive_dir(char drive);
std::string strip_quotes(const std::string &s);
int cmd_cd(int argc, char **argv);

struct Command {
    const char *name;
    command_handler_t handler;
};

struct Help {
    const char *msg;
    const char *cmd;
};

int cmd_help(int argc, char **argv);
int run_command(const char *cmdline, int);
