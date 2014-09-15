include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -fPIC
LFLAGS = $(COMMON_DLFLAGS)
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include
LFLAGS = $(COMMON_DLFLAGS)
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include
LFLAGS = $(COMMON_DLFLAGS)
endif

DEST = ../../../import/image/raw.so
VPATH = ../../common

OBJS = \
	m_image_raw.o \
	pbm.o \
	windows_bmp.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

