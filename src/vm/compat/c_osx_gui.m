#define DEFINE_NO_BOOL
#import <AppKit/NSAlert.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSApplication.h>

#include "c_osx.m"
#include "c_osx_cl_gui.m"
#include "c_posix_osx.c"

/*
@interface AppDelegate : NSObject <NSApplicationDelegate>
- (void)applicationDidFinishLaunching:(NSNotification *)notification;
@end


@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification;
{
//    show_error_message("OK", -1, 0);
}
- (BOOL)openFileWithoutUI:(NSString *)filename;
{
    show_error_message([filename UTF8String], -1, 0);
    return YES;
}

@end

static AppDelegate *s_delegate;
 
*/

///////////////////////////////////////////////////////////

static void cocoa_messagebox(const char *s, int warn)
{
    NSString *nmsg = [[NSString alloc] initWithUTF8String: s];
    NSAlertStyle style = (warn ? NSWarningAlertStyle : NSCriticalAlertStyle);
    NSAlert *alertView = [[NSAlert alloc] init];
    
    [alertView setMessageText:@"fox"];
    [alertView setInformativeText:nmsg];
    [alertView setAlertStyle:style];
    [alertView runModal];
    [nmsg release];
}
void show_error_message(const char *msg, int msg_size, int warn)
{
    char *pz = str_dup_p(msg, msg_size, NULL);
    cocoa_messagebox(pz, warn);
    free(pz);
}

static void appclose_handler(int signum)
{
    exit(1);
}

int main(int argc, char **argv, char **envp)
{
    signal(SIGTERM, appclose_handler);
    signal(SIGINT, appclose_handler);

/*
    NSApplication *application = [NSApplication sharedApplication];
    s_delegate = [AppDelegate new];
    
    [application setDelegate:s_delegate];
    [application run];
    
    [application release];
    [s_delegate release];

    return 0;
 */
    init_fox_vm(RUNNING_MODE_GUI);
    init_env(envp);
    return main_fox(argc, (const char**)argv) ? 0 : 1;
}
