#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "macros.h"

int run_command(char *cmd, int mode) {
    // TODO: Add support for external commands, add support for arguments
    if (strcmp(cmd, "exit") == 0) {
        exit(0);
    }
    else {
        fprintf(stderr, "bad command: %s\n", cmd);
        return 9009;
    }
}
