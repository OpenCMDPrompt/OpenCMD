#include <stdio.h>
#include <stdlib.h>

/* External functions */
extern int shell(void);
extern int dosbatch(void);
extern int ntbatch(void);

/* Entry point */
int main() {
    // For now, hardcode doing shell()
    int exit_code = shell();
    return exit_code;
}
