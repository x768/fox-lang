#ifndef _GUI_COMPAT_H_
#define _GUI_COMPAT_H_


#define NOT_DEFINE_BOOL
#include "m_gui.h"

#ifdef WIN32

#include <windows.h>

#elif defined(__APPLE__)

#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>

#else  /* GTK+ */

#include <gtk/gtk.h>

#endif

#endif /* _GUI_COMPAT_H_ */