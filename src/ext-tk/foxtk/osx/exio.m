#define NOT_DEFINE_BOOL
#include "gui_compat.h"
#include "gui.h"
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSError.h>
#import <AppKit/NSWorkspace.h>


static NSURL *filename_to_nsurl(RefStr *rs)
{
    // NULを含む場合エラー
    if (str_has0(rs->c, rs->size)) {
        fs->throw_errorf(fs->mod_io, "FileOpenError", "File path contains '\\0'");
        return NULL;
    }
    {
        NSString *nstr = [[NSString alloc] initWithUTF8String:rs->c];
        return [NSURL fileURLWithPath:nstr];
    }
}

static NSURL *Value_to_url(Value v)
{
    const RefNode *type = fs->Value_type(v);
    if (type == fs->cls_str || type == fs->cls_file) {
        return filename_to_nsurl(Value_vp(v));
    } else if (type == fs->cls_uri) {
        RefStr *rs = Value_vp(v);
        NSString *nstr = [[NSString alloc] initWithUTF8String:rs->c];
        return [NSURL URLWithString:nstr];
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Str, File or Uri required but %n", type);
        return FALSE;
    }
}

int uri_to_file_sub(Value *dst, Value src)
{
    return TRUE;
}

int exio_trash_sub(Value vdst)
{
    NSURL *dst = Value_to_url(vdst);
    if (dst == NULL) {
        return FALSE;
    }

    NSArray *urls = [NSArray arrayWithObject:dst];
    //dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    void (^handler)(NSDictionary *, NSError *) =
    ^(NSDictionary *newURLs, NSError *error) {
        if (error != nil) {
            NSString *msg = [error localizedDescription];
            fs->throw_errorf(fs->mod_io, "WriteError", "%s", [msg UTF8String]);
        }
        //dispatch_semaphore_signal(semaphore);
    };
    
    [[NSWorkspace sharedWorkspace] recycleURLs:urls
                             completionHandler:handler];
    //dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    //dispatch_release(semaphore);

    return !Value_isref(fg->error);
}
