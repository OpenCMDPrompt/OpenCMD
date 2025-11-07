#include <stdio.h>
#include <stdlib.h>

/* External functions */
extern shell();
extern dosbatch();
extern ntbatch();

/* Entry point */
int main() {
    // For now, hardcode doing shell()
    int exit_code = shell();
    return exit_code;
}
