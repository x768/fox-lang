#include "c_osx.m"
#include "c_posix_osx.c"
#include <string.h>

/**
 * OSで設定されている地域と言語を取得
 */
const char *get_default_locale(void)
{
	return "(neutral)";
}

void get_local_timezone_name(char *buf, int max)
{
	strcpy(buf, "Etc/UTC");
}

int main(int argc, char **argv, char **envp)
{
	const char *argv_fox[3];
    
	init_fox_vm();
	init_env(envp);
    
	argv_fox[0] = argv[0];
	argv_fox[1] = Hash_get(&fs->envs, "PATH_TRANSLATED", -1);
	argv_fox[2] = NULL;
    
	main_fox(argv_fox[1] != NULL ? 2 : 1, argv_fox);
	return 0;
}
