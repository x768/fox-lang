#include "fox_vm.h"
#include "c_posix.c"
#include "c_posix_cl_gui.c"


int main(int argc, char **argv, char **envp)
{
    init_fox_vm();
    init_env(envp);
    return main_fox(argc, (const char**)argv) ? 0 : 1;
}

