#include "c_osx.m"
#include "c_osx_cl_gui.m"
#include "c_posix_osx.c"


int main(int argc, char **argv, char **envp)
{
    init_fox_vm(RUNNING_MODE_CL);
    init_env(envp);
    return main_fox(argc, (const char**)argv) ? 0 : 1;
}
