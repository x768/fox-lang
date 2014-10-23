/*
 * compat_mac.m
 *
 *  Created on: 2012/12/15
 *      Author: frog
 */

#define NOT_DEFINE_BOOL
#include "gui_compat.h"
#include "gui.h"
#include <stdlib.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSArray.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSBitmapImageRep.h>
#import <AppKit/NSPasteboard.h>




int conv_graphics_to_image(Value *vret, GraphicsHandle img)
{
    NSImage *image = img;
    NSBitmapImageRep *imgrep = [NSBitmapImageRep
                                imageRepWithData:[image TIFFRepresentation]];

    RefImage *fi = fs->buf_new(cls_image, sizeof(RefImage));
    int width = [imgrep pixelsWide];
    int height = [imgrep pixelsHigh];
    int pitch = [imgrep bytesPerRow];
    int samples_per_pixel = [imgrep samplesPerPixel];
    uint8_t *data = [imgrep bitmapData];
    int y;

    *vret = vp_Value(fi);
    if ([imgrep bitsPerSample] != 8) {
        fs->throw_errorf(mod_gui, "GuiError", "Format not supported");
        return FALSE;
    }
    
    fi->width = width;
    fi->height = height;
    if (width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
        fs->throw_errorf(mod_gui, "GuiError", "Graphics size too large (max:%d)", MAX_IMAGE_SIZE);
        return FALSE;
    }
    if (samples_per_pixel == 4) {
        fi->bands = BAND_RGBA;
        fi->pitch = width * 4;
    } else if (samples_per_pixel == 3) {
        fi->bands = BAND_RGB;
        fi->pitch = width * 3;
    } else {
        fs->throw_errorf(mod_gui, "GuiError", "Graphics size too large (max:%d)", MAX_IMAGE_SIZE);
        return FALSE;
    }
    fi->data = malloc(fi->pitch * height);

    for (y = 0; y < height; y++) {
        uint8_t *dst = fi->data + y * fi->pitch;
        const uint8_t *src = data + y * pitch;
        memcpy(dst, src, fi->pitch);
    }
    
    return TRUE;
}
GraphicsHandle conv_image_to_graphics(RefImage *img)
{
    int samples_per_pixel;
    NSBitmapImageRep *nsrep = nil;
    int has_alpha = FALSE;
    int pitch;
    uint8_t **data;
    int y;

    if (img->bands == BAND_RGB) {
        samples_per_pixel = 3;
        pitch = img->width * 3;
    } else if (img->bands == BAND_RGBA) {
        samples_per_pixel = 4;
        pitch = img->width * 4;
        has_alpha = TRUE;
    } else {
        fs->throw_errorf(mod_image, "ImageError", "Not supported type");
        return NULL;
    }

    data = malloc(img->height * sizeof(uint8_t*));
    for (y = 0; y < img->height; y++) {
        data[y] = img->data + y * img->pitch;
    }

    nsrep = [NSBitmapImageRep alloc];
    nsrep = [nsrep initWithBitmapDataPlanes:data
                                 pixelsWide:img->width
                                 pixelsHigh:img->height
                              bitsPerSample:8
                            samplesPerPixel:samples_per_pixel
                                   hasAlpha:has_alpha
                                   isPlanar:NO
                             colorSpaceName:NSCalibratedRGBColorSpace
                               bitmapFormat:0
                                bytesPerRow:pitch
                               bitsPerPixel:samples_per_pixel * 8];
    free(data);

    if (nsrep == nil) {
        fs->throw_errorf(mod_image, "ImageError", "Cannot initialize NSBitmapImageRep");
        return NULL;
    }
    
    return nsrep;
}

int clipboard_clear()
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    return TRUE;
}

int clipboard_is_available(const RefNode *klass)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *types = nil;

    if (klass == fs->cls_str) {
        types = [NSArray arrayWithObject: NSPasteboardTypeString];
    } else if (klass == fs->cls_file) {
        types = [NSArray arrayWithObject: NSURLPboardType];
    } else if (klass == cls_image) {
        types = [NSArray arrayWithObject: NSPasteboardTypeTIFF];
    }

    if (types != nil) {
        return [pasteboard availableTypeFromArray:types] != nil;
    } else {
        return FALSE;
    }
}

Value clipboard_get_text()
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSString *str = [pasteboard stringForType:NSPasteboardTypeString];

    if (str != nil) {
        char *p = (char*)[str UTF8String];
        return fs->cstr_Value(fs->cls_str, p, -1);
    } else {
        return VALUE_NULL;
    }
}
void clipboard_set_text(const char *src_p, int src_size)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    char *psz = fs->str_dup_p(src_p, src_size, NULL);
    NSString *nstr = [[NSString alloc] initWithUTF8String:psz];

    [pasteboard clearContents];
    [pasteboard setString:nstr forType:NSPasteboardTypeString];
    free(psz);
}
static Value NSURL_to_Value(NSURL *url, int is_uri)
{
    if (is_uri) {
        NSString *nstr = [url absoluteString];
        const char *str = [nstr UTF8String];
        return fs->cstr_Value(fs->cls_uri, str, -1);
    } else {
        const char *path = [url fileSystemRepresentation];
        return fs->cstr_Value(fs->cls_file, path, -1);
    }
}
int clipboard_get_files(Value *v, int uri)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *types = [NSArray arrayWithObject:[NSURL class]];
    NSDictionary *options = [NSDictionary dictionary];
    
    BOOL ok = [pasteboard
               canReadObjectForClasses:types
               options:options];
    
    if (ok) {
        NSArray *objects = [pasteboard
                            readObjectsForClasses:types
                            options:options];
        int i;
        int n = [objects count];
        RefArray *afile = fs->refarray_new(0);
        *v = vp_Value(afile);

        for (i = 0; i < n; i++) {
            NSURL *url = [objects objectAtIndex:i];
            Value *vf = fs->refarray_push(afile);
            *vf = NSURL_to_Value(url, uri);
        }
    }

    return TRUE;
}
int clipboard_set_files(Value *v, int num)
{
    return TRUE;
}
int clipboard_get_image(Value *v)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *types = [NSArray arrayWithObject:[NSImage class]];
    NSDictionary *options = [NSDictionary dictionary];

    BOOL ok = [pasteboard
               canReadObjectForClasses:types
               options:options];
    
    if (ok) {
        NSArray *objects = [pasteboard
                            readObjectsForClasses:types
                            options:options];
        NSImage *image = [objects objectAtIndex:0];
        conv_graphics_to_image(v, image);
    }
    
    return TRUE;
}
int clipboard_set_image(RefImage *img)
{
    NSPasteboard *pasteboard;
    NSBitmapImageRep *nsimg = conv_image_to_graphics(img);

    if (nsimg == NULL) {
        return FALSE;
    }

    pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard declareTypes:[NSArray arrayWithObjects:NSPasteboardTypeTIFF, nil] owner:nil];
    [pasteboard setData:[nsimg TIFFRepresentation] forType:NSTIFFPboardType];

    return TRUE;
}
