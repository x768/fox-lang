include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -I./giflib -fPIC
LFLAGS = $(COMMON_DLFLAGS) -lpng -ljpeg
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include -I./giflib
LFLAGS = $(COMMON_DLFLAGS) -L/usr/local/lib -lpng -ljpeg /usr/local/lib/libz.a
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include -I../../lib/libpng -I../../lib/jpeg-8d -I./giflib
LFLAGS = $(COMMON_DLFLAGS) ../../lib/libpng/.libs/libpng15.a ../../lib/jpeg-8d/.libs/libjpeg.a -lz
endif

DEST = ../../../import/image/webimage.so
VPATH = ../../common:./giflib

OBJS = \
	m_webimage.o \
	strutil.o \
	dgif_lib.o \
	egif_lib.o \
	gif_err.o \
	gif_hash.o \
	gifalloc.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)
