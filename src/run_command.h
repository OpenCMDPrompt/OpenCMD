#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "macros.h"

int run_command(char *cmd, int mode) {
    fprintf(stderr, "bad command: %s\n", cmd);
    return 127;
}
