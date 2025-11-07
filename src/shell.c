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

int shell() {
    printf("OpenCMD %s. Visit https://www.gnu.org/licenses/gpl-3.0.en.html#license-text for the license text.\n", VERSION);
    
    /* Shell loop */
    int last_error_code;
    char *input;

    char cwd[MAX_PATH];
    DWORD len = GetCurrentDirectoryA(sizeof(cwd), cwd);
    if (len == 0) {
        printf("GetCurrentDirectory failed (error %lu)\n", GetLastError());
        return 1;
    }
    
    char prompt[PATH_MAX + 4];
    snprintf(prompt, sizeof(prompt), "%s>", cwd);
    
    while ((input = readline(prompt)) != NULL) {
        if (*input) {
            int state = run_command(input, 0);
            add_history(input);
            last_error_code = state;
        }
        free(input); // Must free memory allocated by readline
    }

    return last_error_code;
}
