include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

CFLAGS = -I../../include -I/usr/local/include
LFLAGS = $(COMMON_DLFLAGS) -L/usr/local/lib -lole32 -loleaut32 -luuid

DEST = ../../../import/windows.so
VPATH = ../../common


OBJS = \
	m_windows.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

