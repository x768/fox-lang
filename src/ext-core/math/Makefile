include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -fPIC -DMP_ARGCHK=0
LFLAGS = $(COMMON_DLFLAGS) -lm
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include -DMP_ARGCHK=0
LFLAGS = $(COMMON_DLFLAGS) -lm
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include -DMP_ARGCHK=0
LFLAGS = $(COMMON_DLFLAGS) -lm
endif

DEST = ../../../import/math.so
VPATH = ../../common


OBJS = \
	m_math.o \
	mpi.o \
	mpi2.o \
	mplogic.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

