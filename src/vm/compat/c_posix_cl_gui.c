#include <stdlib.h>
#include <string.h>
#include <fcntl.h>


/**
 * OSで設定されている地域と言語を取得
 */
const char *get_default_locale(void)
{
    static const char *def_locale = NULL;

    if (def_locale == NULL) {
        const char *p = Hash_get(&fs->envs, "LANG", -1);
        if (p != NULL) {
            int i;
            char *loc = Mem_get(&fg->st_mem, strlen(p));
            for (i = 0; p[i] != '\0' && p[i] != '.'; i++) {
                loc[i] = p[i];
            }
            loc[i] = '\0';
            def_locale = loc;
        } else {
            def_locale = "(neutral)";
        }
    }
    return def_locale;
}

void get_local_timezone_name(char *buf, int max)
{
    const char *p = Hash_get(&fs->envs, "TZ", -1);

    if (p != NULL) {
        int len = strlen(p);
        if (len >= max) {
            len = max - 1;
        }
        memcpy(buf, p, len);
        buf[len] = '\0';
    } else {
        int f = open_fox("/etc/timezone", O_RDONLY, DEFAULT_PERMISSION);
        if (f != -1) {
            int size = read_fox(f, buf, max - 1);
            close_fox(f);

            while (size > 0 && buf[size - 1] == '\n') {
                size--;
            }
            buf[size] = '\0';
        }
    }
}


