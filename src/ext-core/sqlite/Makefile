include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -fPIC
LFLAGS = $(COMMON_DLFLAGS) -lsqlite3
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I../../lib/sqlite3
LFLAGS = $(COMMON_DLFLAGS) ../../lib/sqlite3/sqlite3.o
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include
LFLAGS = $(COMMON_DLFLAGS) -lsqlite3
endif

DEST = ../../../import/db/sqlite3.so
VPATH = ../../common


OBJS = \
	m_sqlite.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

