#pragma comment(lib, "Advapi32.lib")

#include <cstdlib>

// External functions
extern int shell();
extern int dosbatch();
extern int ntbatch();

// Entry point
int main() {
    // For now, hardcode calling shell()
    int exit_code = shell();
    return exit_code;
}
