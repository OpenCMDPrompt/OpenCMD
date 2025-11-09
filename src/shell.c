#include <stdio.h>
#include <stdlib.h>
#include "macros.h"
#include "run_command.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <limits.h>
#include <windows.h>

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

char *custom_prompt = NULL;

int shell() {
    printf("OpenCMD %s. Visit https://www.gnu.org/licenses/gpl-3.0.en.html#license-text.\n", VERSION);

    int last_error_code = 0;
    char *input;
    char cwd[MAX_PATH];

    while (1) {
        DWORD len = GetCurrentDirectoryA(sizeof(cwd), cwd);
        if (len == 0) {
            printf("GetCurrentDirectory failed (error %lu)\n", (unsigned long)GetLastError());
            return 1;
        }

        /* Build the prompt dynamically */
        char prompt[PATH_MAX + 16];
        if (custom_prompt) {
            snprintf(prompt, sizeof(prompt), "%s", custom_prompt);
        } else {
            snprintf(prompt, sizeof(prompt), "%s>", cwd);
        }
        SetCurrentDirectoryA(cwd);
        input = readline(prompt);
        if (!input) break;

        if (*input) {
            int state = run_command(input, SHELL_MODE);
            add_history(input);
            last_error_code = state;
        }

        free(input);
    }

    return last_error_code;
}
