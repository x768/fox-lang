include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include `sdl2-config --cflags` -fPIC
LFLAGS = $(COMMON_DLFLAGS) `sdl2-config --libs`
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include `sdl2-config --cflags`
LFLAGS = $(COMMON_DLFLAGS) `sdl2-config --libs`
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include `sdl2-config --cflags`
LFLAGS = $(COMMON_DLFLAGS) `sdl2-config --libs` -framework Foundation -framework AppKit
endif

DEST = ../../../import/sdl2.so
VPATH = ../../common


OBJS = \
	m_sdl.o \
	sdl_gl.o \



all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)
