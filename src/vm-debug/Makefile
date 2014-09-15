include ../Makefile.env
include ../Makefile.common

.PHONY: clean

LOCAL_CFLAGS = -I../vm/src -I../vm/compat -I../include -I..

ifeq ($(TARGET),posix)
CFLAGS = -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_LFS64_LARGEFILE=1 -DDEBUGGER -DMP_ARGCHK=0 $(LOCAL_CFLAGS)
LFLAGS = -lrt -ldl -lpcre -lreadline
endif

ifeq ($(TARGET),win)
CFLAGS = -I/usr/local/include -I../lib/readline/include -DPCRE_STATIC -DDEBUGGER -DMP_ARGCHK=0 $(LOCAL_CFLAGS)
LFLAGS = -L/usr/local/lib -lws2_32 /usr/local/lib/libpcre.a ../lib/readline/lib/libreadline6.dll.a -liconv
endif

ifeq ($(TARGET),osx)
CFLAGS = -I../lib/libpcre -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_LFS64_LARGEFILE=1 -DDEBUGGER -DMP_ARGCHK=0 $(LOCAL_CFLAGS)
LFLAGS = -L../lib/libpcre -ldl -liconv -lpcre -lreadline -framework Foundation
endif


VPATH = src:../vm/src:../vm/compat:../common


OBJS_COMMON = \
	mpi.o \
	mpi2.o \
	mplogic.o \
	strutil.o \
	compat_$(TARGET).o \
	vm.o \
	col.o \
	file.o \
	util.o \
	util_str.o \
	value.o \
	datetime.o \
	lex.o \
	parse.o \
	exec.o \
	throw.o \
	m_cgi.o \
	m_charset.o \
	m_col.o \
	m_file.o \
	m_io.o \
	m_lang.o \
	m_locale.o \
	m_map.o \
	m_marshal.o \
	m_mime.o \
	m_number.o \
	m_str.o \
	m_time.o \
	interactive.o \



OBJS_CL = \
	$(OBJS_COMMON) \
	main_cl.o \
	c_$(TARGET)_cl.o


all: $(OBJS_CL)
	gcc $(OBJS_CL) -o ../../bin/foxd $(LFLAGS) $(COMMON_LFLAGS)


.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)


clean:
	-rm *.o


