include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -fPIC -I../../lib/mariadb/include
LFLAGS = $(COMMON_DLFLAGS) -L../../lib/mariadb/libmariadb -lmariadbclient
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include -I../../lib/mariadb/include
LFLAGS = $(COMMON_DLFLAGS) -L../../lib/mariadb/libmariadb -lmariadbclient -lws2_32 -liphlpapi
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include -I../../lib/mariadb/include
LFLAGS = $(COMMON_DLFLAGS) -L../../lib/mariadb/libmariadb -lmariadbclient
endif

DEST = ../../../import/db/mysql.so
VPATH = ../../common

OBJS = \
	m_mysql.o \
	strutil.o \
	compat_$(TARGET).o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

