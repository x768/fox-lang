#define DEFINE_NO_BOOL
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSLocale.h>

#include "config.h"
#include "fox_vm.h"
#include <unistd.h>
#include <time.h>
#include <mach-o/dyld.h>


static int64_t NSDate_to_int64(NSDate *dt)
{
    NSTimeInterval interval = [dt timeIntervalSince1970];
    return (int64_t)((double)interval * 1000.0);
}

int64_t get_now_time()
{
    NSDate *now = [NSDate dateWithTimeIntervalSinceNow: 0.0f];
    if (now != NULL) {
        return NSDate_to_int64(now);
    } else {
        return 0;
    }
}

const char *get_fox_home()
{
#ifdef FOX_HOME
    return FOX_HOME;
#else
    enum {
        FOX_PATH_MAX = 512,
    };
    char *path = Mem_get(&fg->st_mem, FOX_PATH_MAX);
    char *p;
#ifdef FOX_WINDOW
    int n = 5;
#else
    int n = 2;
#endif
    int len;
    uint32_t size = FOX_PATH_MAX;
    
    memset(path, 0, size);
    if (_NSGetExecutablePath(path, &size) != 0) {
        return "./";
    }
    len = strlen(path);
    len = make_path_regularize(path, len);
    
    p = path + len;
    while (p > path) {
        p--;
        if (*p == '/') {
            *p = '\0';
            n--;
            if (n <= 0) {
                break;
            }
        }
    }
    return path;
#endif
}

