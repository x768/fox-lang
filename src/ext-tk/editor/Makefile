include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include `pkg-config --cflags gtk+-3.0` -fPIC
LFLAGS = $(COMMON_DLFLAGS) `pkg-config --libs gtk+-3.0`
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include
LFLAGS = $(COMMON_DLFLAGS)
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include
LFLAGS = $(COMMON_DLFLAGS) -framework Foundation -framework AppKit
endif

DEST = ../../../import/foxtk/editor.so
VPATH = ../../common


OBJS = \
	m_editor.o \
	strutil.o \
	editor_$(TARGET).o \



all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)
