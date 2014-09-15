include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -fPIC
LFLAGS = $(COMMON_DLFLAGS) -lssl
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I../../lib/openssl/include
LFLAGS = $(COMMON_DLFLAGS) -L../../lib/openssl -lssl -lcrypto -lws2_32 -lgdi32
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include
LFLAGS = $(COMMON_DLFLAGS) -lssl -lcrypto
endif

DEST = ../../../import/crypt.so
VPATH = ../../common


OBJS = \
	m_crypt.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

