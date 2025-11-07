#include <stdio.h>
#include <stdlib.h>
#include "macros.h"
#include "run_command.h"
#include <readline/readline.h>
#include <readline/history.h>

int shell() {
    printf("OpenCMD %s. Visit https://www.gnu.org/licenses/gpl-3.0.en.html#license-text for the license text.\n", VERSION);
    
    /* Shell loop */
    int last_error_code;
    char *input;

    while ((input = readline("OpenCMD> ")) != NULL) {
        if (*input) {
            int state = run_command(input, 0);
            add_history(input);
            last_error_code = state;
        }
        free(input); // Must free memory allocated by readline
    }

    return last_error_code;
}
