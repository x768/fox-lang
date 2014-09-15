include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = $(COMMON_CFLAGS) -I../../include `pkg-config --cflags gtk+-3.0` -fPIC
LFLAGS = $(COMMON_DLFLAGS) `pkg-config --libs gtk+-3.0`
endif
ifeq ($(TARGET),win)
CFLAGS = $(COMMON_CFLAGS) -I../../include -I/usr/local/include
LFLAGS = $(COMMON_DLFLAGS) -mwindows
endif
ifeq ($(TARGET),osx)
CFLAGS = $(COMMON_CFLAGS) -I../../include
LFLAGS = $(COMMON_DLFLAGS) -framework Foundation -framework AppKit
endif

DEST = ../../../import/foxtk.so
VPATH = ../../common


OBJS = \
	m_gui.o \
	strutil.o \
	m_gui_$(TARGET).o \
	clipboard_$(TARGET).o \
	dialog_$(TARGET).o \
	exio_$(TARGET).o \
	form_$(TARGET).o \
	menu_$(TARGET).o \



all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS)
