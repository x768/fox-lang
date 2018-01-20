#define NOT_DEFINE_BOOL
#include "fox_macos.h"
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSAppleScript.h>
#import <Foundation/NSAppleEventManager.h>


int apple_script_new_sub(RefAppleScript *r, const char *src)
{
    int result = TRUE;
    NSString *nssrc = [[NSString alloc] initWithUTF8String: src];
    NSAppleScript *script = [[NSAppleScript alloc] initWithSource:nssrc];
    NSDictionary *error = nil;

    [script compileAndReturnError:&error];
    if (error != nil) {
        NSString *message = [error objectForKey:@"NSAppleScriptErrorMessage"];
        fs->throw_errorf(mod_macos, "AppleScriptError", "%s", [message UTF8String]);
        [error release];
        result = FALSE;
    } else {
        r->apple_script = script;
    }
    [nssrc release];

    return result;
}
void apple_script_close_sub(RefAppleScript *r)
{
    NSAppleScript *script = r->apple_script;

    if (script != nil) {
        [script release];
        r->apple_script = nil;
    }
}
int apple_script_exec_sub(RefAppleScript *r)
{
    NSAppleScript *script = r->apple_script;
    if (script != nil) {
        NSDictionary *error = nil;

        [script executeAndReturnError:&error];
        if (error != nil) {
            NSString *message = [error objectForKey:@"NSAppleScriptErrorMessage"];
            fs->throw_errorf(mod_macos, "AppleScriptError", "%s", [message UTF8String]);
            [error release];
            return FALSE;
        }
    }
    return TRUE;
}

void output_nslog_sub(const char *src)
{
    NSLog(@"%s", src);
}
