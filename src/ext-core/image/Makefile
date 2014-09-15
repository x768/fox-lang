include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -fPIC
LFLAGS = $(COMMON_DLFLAGS) -lm
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include -Wl,--version-script,../../fox-ext.map
LFLAGS = $(COMMON_DLFLAGS) -lm
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include
LFLAGS = $(COMMON_DLFLAGS) -lm
endif

DEST = ../../../import/image.so
VPATH = ../../common

OBJS = \
	m_image.o \
	imgutil.o \
	quantize.o \
	strutil.o \
	compat_$(TARGET).o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

