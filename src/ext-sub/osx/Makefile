include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

CFLAGS = -I../../include
LFLAGS = $(COMMON_DLFLAGS) -framework Foundation -framework AppKit

DEST = ../../../import/osx.so
VPATH = ../../common


OBJS = \
	m_osx.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

