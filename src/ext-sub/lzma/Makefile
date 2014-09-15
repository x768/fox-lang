include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -fPIC
LFLAGS = $(COMMON_DLFLAGS) -llzma
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include -I../../lib/xz/src/liblzma/api
LFLAGS = $(COMMON_DLFLAGS) ../../lib/xz/src/liblzma/.libs/liblzma.a
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include -I../../lib/xz/src/liblzma/api
LFLAGS = $(COMMON_DLFLAGS) ../../lib/xz/src/liblzma/.libs/liblzma.a
endif

DEST = ../../../import/archive/lzma.so
VPATH = ../../common

OBJS = \
	m_lzma.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)
