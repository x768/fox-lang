include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -I../../lib/libvpx -fPIC
LFLAGS = $(COMMON_DLFLAGS) ../../lib/libvpx/libvpx.a -lvorbis
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include -I../../lib/libvpx
LFLAGS = $(COMMON_DLFLAGS) -L/usr/local/lib ../../lib/libvpx/libvpx.a -lpthread ../../lib/libvorbis/libvorbis.a ../../lib/libvorbis/libvorbisenc.a ../../lib/libvorbis/libvorbisfile.a ../../lib/libogg/libogg.a
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include -I../../lib/libvpx
LFLAGS = $(COMMON_DLFLAGS) ../../lib/libvpx/libvpx.a -lvorbis
endif

DEST = ../../../import/media/webm.so
VPATH = ../../common

OBJS = \
	m_webm.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

