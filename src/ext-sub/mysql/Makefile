include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -fPIC `mysql_config --cflags`
LFLAGS = $(COMMON_DLFLAGS) `mysql_config --libs`
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include -I../lib/mysql-connector/include
LFLAGS = $(COMMON_DLFLAGS) -L../lib/mysql-connector/lib-mingw -lmysql
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include `mysql_config --cflags`
LFLAGS = $(COMMON_DLFLAGS) `mysql_config --libs`
endif

DEST = ../../../import/db/mysql.so
VPATH = ../../common

OBJS = \
	m_mysql.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

