include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean


OBJS_COMMON = \
	m_net.o \
	strutil.o \
	compat_$(TARGET).o \


ifeq ($(TARGET),posix)
CFLAGS = -I../../include -fPIC
LFLAGS = $(COMMON_DLFLAGS)
OBJS = \
	$(OBJS_COMMON) \
	net_posix.o \

endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include
LFLAGS = $(COMMON_DLFLAGS) -lws2_32 -liphlpapi
OBJS = \
	$(OBJS_COMMON) \
	net_win.o \

endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include
LFLAGS = $(COMMON_DLFLAGS)
OBJS = \
	$(OBJS_COMMON) \
	net_posix.o \

endif


DEST = ../../../import/io/net.so
VPATH = ../../common



all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)

