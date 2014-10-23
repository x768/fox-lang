#define DEFINE_NO_BOOL
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSTimeZone.h>

#include <string.h>
#include <stdlib.h>


void get_local_timezone_name(char *buf, int max)
{
    NSTimeZone *tz = [NSTimeZone defaultTimeZone];
    if (tz != nil) {
        NSString *nstr = [tz name];
        const char *p = [nstr UTF8String];
        if (strlen(p) < max) {
            strcpy(buf, p);
        } else {
            memcpy(buf, p, max - 1);
            buf[max - 1] = '\0';
        }
    } else {
        strcpy(buf, "Etc/UTC");
    }
}

/**
 * OSで設定されている地域と言語を取得
 */
const char *get_default_locale(void)
{
    static const char *def_locale = NULL;
    
    if (def_locale == NULL) {
        NSLocale *loc = [NSLocale currentLocale];
        NSString *name = [loc localeIdentifier];
        const char *cstr = [name UTF8String];

        char *p = malloc(strlen(cstr) + 1);
        strcpy(p, cstr);
        def_locale = p;
    }
    return def_locale;
}
