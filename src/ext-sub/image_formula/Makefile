include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -fPIC
LFLAGS = $(COMMON_DLFLAGS) -lm
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include
LFLAGS = $(COMMON_DLFLAGS) -lm
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include
LFLAGS = $(COMMON_DLFLAGS) -lm
endif

DEST = ../../../import/image/formula.so
VPATH = ../../common

OBJS = \
	m_formula.o \
	texfonts.o \
	mimetex.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

