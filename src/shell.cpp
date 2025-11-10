#include <iostream>
#include <string>
#include <memory>
#include <windows.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "macros.hpp"
#include "run_command.hpp"

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

bool echo_enabled = true;
std::string custom_prompt;

int shell() {
    std::cout << "OpenCMD " << VERSION 
              << ". Visit https://www.gnu.org/licenses/gpl-3.0.en.html#license-text.\n";

    int last_error_code = 0;

    rl_bind_key('\t', rl_complete);

    while (true) {
        char cwd[PATH_MAX];
        DWORD len = GetCurrentDirectoryA(sizeof(cwd), cwd);
        if (len == 0) {
            std::cerr << "GetCurrentDirectory failed (error " 
                      << GetLastError() << ")\n";
            return 1;
        }

        // Build the prompt dynamically
        std::string prompt;
        if (!custom_prompt.empty()) {
            prompt = custom_prompt;
        } else {
            prompt = std::string(cwd) + ">";
        }
        if (!echo_enabled) {
            prompt = "";
        }

        SetCurrentDirectoryA(cwd);

        std::unique_ptr<char, decltype(&std::free)> input(readline(prompt.c_str()), &std::free);
        if (!input) break;

        if (*input) {
            int state = run_command(input.get(), SHELL_MODE);
            add_history(input.get());
            last_error_code = state;
        }
    }

    return last_error_code;
}
