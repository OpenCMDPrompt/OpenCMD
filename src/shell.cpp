#include "macros.hpp"
#include "run_command.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <windows.h>

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

bool echo_enabled = true;
std::string custom_prompt;

int shell() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::ios_base::sync_with_stdio(false);
    std::setlocale(LC_ALL, ".UTF-8");

    std::cout << "OpenCMD " << VERSION
              << ". Visit https://www.gnu.org/licenses/gpl-3.0.en.html#license-text.\n";

    int last_error_code = 0;

    while (true) {
        char cwd[PATH_MAX];
        DWORD len = GetCurrentDirectoryA(sizeof(cwd), cwd);
        if (len == 0) {
            std::cerr << "GetCurrentDirectory failed (error " << GetLastError() << ")\n";
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

        std::cout << prompt;

        std::string input;
        if (!std::getline(std::cin, input)) {
            break;
        }

        if (!input.empty()) {
            int state = run_command(input.c_str(), SHELL_MODE);
            last_error_code = state;
        }
    }

    return last_error_code;
}
