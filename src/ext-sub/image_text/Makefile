include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include `freetype-config --cflags` -fPIC
LFLAGS = $(COMMON_DLFLAGS) `freetype-config --libs`
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include `freetype-config --cflags`
LFLAGS = $(COMMON_DLFLAGS) `freetype-config --libs` /usr/local/lib/libz.a
endif
ifeq ($(TARGET),osx)
CFLAGS = -Wshadow -I../../include -I../../lib/freetype/include
LFLAGS = $(COMMON_DLFLAGS) ../../lib/freetype/objs/.libs/libfreetype.a ../../lib/libpng/.libs/libpng15.a -lz -lbz2
endif

DEST = ../../../import/image/text.so
VPATH = ../../common

OBJS = \
	m_image_text.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

