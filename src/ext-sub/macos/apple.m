#define NOT_DEFINE_BOOL
#include "fox_macos.h"
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSAppleScript.h>
#import <Foundation/NSAppleEventManager.h>


int exec_apple_script(const char *src)
{
    NSString *nssrc = [[NSString alloc] initWithUTF8String: src];
    NSDictionary *error = nil;
    NSAppleScript *script = [[NSAppleScript alloc] initWithSource:nssrc];
    int result = TRUE;

    [script executeAndReturnError:&error];
    if (error != nil) {
        NSString *message = [error objectForKey:@"NSAppleScriptErrorMessage"];
        fs->throw_errorf(mod_macos, "AppleScriptError", "%s", [message UTF8String]);
        result = FALSE;
    }

    [script release];
    return result;
}
