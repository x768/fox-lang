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

DEST = ../../../import/datafmt/json.so
VPATH = ../../common

OBJS = \
	m_json.o \
	strutil.o \
	mpi.o \
	mpi2.o \
	mplogic.o \
	compat_$(TARGET).o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)
