include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -I../../lib/libwebp/src -fPIC
LFLAGS = $(COMMON_DLFLAGS) ../../lib/libwebp/src/.libs/libwebp.a
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include -I../../lib/libwebp/src
LFLAGS = $(COMMON_DLFLAGS) -L/usr/local/lib ../../lib/libwebp/src/.libs/libwebp.a
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include -I../../lib/libwebp/include
LFLAGS = $(COMMON_DLFLAGS) ../../lib/libwebp/lib/libwebp.a
endif

DEST = ../../../import/image/webp.so
VPATH = ../../common

OBJS = \
	m_webp.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

