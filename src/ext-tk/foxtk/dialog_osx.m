/*
 * dialog_osx.m
 *
 *  Created on: 2012/12/23
 *      Author: frog
 */

#define NOT_DEFINE_BOOL
#include "gui_compat.h"
#include "gui.h"
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <AppKit/NSPasteboard.h>
#import <AppKit/NSAlert.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSOpenPanel.h>
#import <AppKit/NSSavePanel.h>


static NSString *cstr_to_nsstring(const char *s, int size)
{
    char *psz = fs->str_dup_p(s, size, NULL);
    NSString *ns = [[NSString alloc] initWithUTF8String: psz];
    free(psz);
    return ns;
}

// TODO: use parent
void alert_dialog(RefStr *msg, RefStr *title, WndHandle parent)
{
    NSString *nmsg = cstr_to_nsstring(msg->c, msg->size);
    NSString *ntitle;
    NSAlert *alertView = [[NSAlert alloc] init];

    if (title != NULL) {
        ntitle = cstr_to_nsstring(title->c, title->size);
    } else {
        ntitle = cstr_to_nsstring("fox", 3);
    }

    [alertView setMessageText:ntitle];
    [alertView setInformativeText:nmsg];
    [alertView setAlertStyle:NSWarningAlertStyle];
    [alertView runModal];
    [ntitle release];
    [nmsg release];
}
// TODO: use parent
int confirm_dialog(RefStr *msg, RefStr *title, WndHandle parent)
{
    NSString *nmsg = cstr_to_nsstring(msg->c, msg->size);
    NSString *ntitle;
    int ret;
    NSAlert *alertView = [[NSAlert alloc] init];

    if (title != NULL) {
        ntitle = cstr_to_nsstring(title->c, title->size);
    } else {
        ntitle = cstr_to_nsstring("fox", 3);
    }

    [alertView setMessageText:ntitle];
    [alertView setInformativeText:nmsg];
    [alertView addButtonWithTitle: @"OK"];
    [alertView addButtonWithTitle: @"Cancel"];

    ret = [alertView runModal];
    [ntitle release];
    [nmsg release];
    return ret == NSAlertFirstButtonReturn;
 }

/**
 * *.bmp;*.jpg -> "*.bmp;*.jpg"
 * Image file:*.bmp;*.jpg -> "*.bmp;*.jpg"
 */
static void split_filter_name(Str *filter, Str src)
{
	int i;
    
	for (i = src.size - 1; i >= 0; i--) {
		if (src.p[i] == ':') {
			filter->p = src.p + i + 1;
			filter->size = src.size - i - 1;
			return;
		}
	}
	*filter = src;
}

static void nsarray_add_filter(NSMutableArray *arr, int *other, Str filter)
{
	const char *p = filter.p;
	const char *end = filter.p + filter.size;
    
	while (p < end) {
		const char *top = p;
		while (p < end && *p != ';') {
			p++;
		}
		if (p > top) {
			Str s = Str_new(top, p - top);
            if (s.size > 2 && s.p[0] == '*' && s.p[1] == '.') {
                if (s.size == 3 && s.p[2] == '*') {
                    *other = TRUE;
                } else {
                    s.p += 2;
                    s.size -= 2;
                    [arr addObject:cstr_to_nsstring(s.p, s.size)];
                }
            } else if (s.size == 1 && s.p[0] == '*') {
                *other = TRUE;
            }
		}
		if (p < end) {
			// ;
			p++;
		}
	}
}
static NSMutableArray *make_filter_string(int *other, RefArray *r)
{
    NSMutableArray *arr = [[NSMutableArray alloc] init];
	int i;
    
	for (i = 0; i < r->size; i++) {
        Str filter;
        split_filter_name(&filter, fs->Value_str(r->p[i]));
        nsarray_add_filter(arr, other, filter);
    }
    return arr;
}
static Value NSURL_to_file_Value(NSURL *url)
{
    const char *path = [url fileSystemRepresentation];
    return fs->cstr_Value(fs->cls_file, path, -1);
}

int file_open_dialog(Value *vret, Str title, RefArray *filter, WndHandle parent, int type)
{
    NSString *ntitle = cstr_to_nsstring(title.p, title.size);
	
    if (type == FILEOPEN_SAVE) {
        NSSavePanel *panel = [NSSavePanel savePanel];
        if (filter != NULL) {
            int other = FALSE;
            NSMutableArray *ns_filter = make_filter_string(&other, filter);
            [panel setAllowedFileTypes:ns_filter];
            if (!other) {
                [panel setAllowedFileTypes:ns_filter];
            }
        }
        [panel setTitle:ntitle];
        if ([panel runModal] == NSFileHandlingPanelOKButton) {
            NSURL *url = [panel URL];
            *vret = NSURL_to_file_Value(url);
        }
    } else {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        //int ret;
        if (filter != NULL) {
            int other = FALSE;
            NSMutableArray *ns_filter = make_filter_string(&other, filter);
            [panel setAllowedFileTypes:ns_filter];
            if (!other) {
                [panel setAllowedFileTypes:ns_filter];
            }
        }
        if (type == FILEOPEN_DIR) {
            [panel setCanChooseDirectories:YES];
            [panel setCanChooseFiles:NO];
        } else if (type == FILEOPEN_OPEN_MULTI) {
            [panel setAllowsMultipleSelection:YES];
        }
        [panel setTitle:ntitle];
        if ([panel runModal] == NSFileHandlingPanelOKButton) {
            NSArray *ns_arr = [panel URLs];
            if (type == FILEOPEN_OPEN_MULTI) {
                int i;
                int n = [ns_arr count];
                RefArray *aret = fs->refarray_new(0);
                *vret = vp_Value(aret);
                for (i = 0; i < n; i++) {
					Value *vf = fs->refarray_push(aret);
                    *vf = NSURL_to_file_Value([ns_arr objectAtIndex:i]);
                }
            } else {
                *vret = NSURL_to_file_Value([ns_arr firstObject]);
            }
        }
    }
    return TRUE;
}

