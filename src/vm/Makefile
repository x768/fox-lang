include ../Makefile.env
include ../Makefile.common

.PHONY: cl
.PHONY: cgi
.PHONY: gui
.PHONY: clean

LOCAL_CFLAGS = -I./src -I./compat -I../include -I..

ifeq ($(TARGET),posix)
CFLAGS = -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_LFS64_LARGEFILE=1 -DMP_ARGCHK=0 $(LOCAL_CFLAGS)
LFLAGS = -lrt -ldl -lpcre
GUI_CFLAGS = `pkg-config --cflags gtk+-3.0`
GUI_LFLAGS = `pkg-config --libs gtk+-3.0`
GUI_DEST = ../../bin/foxw
endif

ifeq ($(TARGET),win)
CFLAGS = -I/usr/local/include -DPCRE_STATIC -DMP_ARGCHK=0 $(LOCAL_CFLAGS)
LFLAGS = -L/usr/local/lib /usr/local/lib/libpcre.a -liconv
GUI_CFLAGS =
GUI_LFLAGS = -mwindows
GUI_DEST = ../../bin/foxw
endif

ifeq ($(TARGET),osx)
CFLAGS = -I../lib/libpcre -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_LFS64_LARGEFILE=1 -DMP_ARGCHK=0 $(LOCAL_CFLAGS)
LFLAGS = -L../lib/libpcre -ldl -liconv -lpcre -framework Foundation
GUI_CFLAGS =
GUI_LFLAGS = -framework AppKit
GUI_DEST = ../../bin/foxw.app/Contents/MacOS/foxw
endif


VPATH = ./src:./compat:../common


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



OBJS_CL = \
	$(OBJS_COMMON) \
	main_cl.o \
	c_$(TARGET)_cl.o


OBJS_CGI = \
	$(OBJS_COMMON) \
	main_cgi.o \
	c_$(TARGET)_cgi.o


OBJS_GUI = \
	$(OBJS_COMMON) \
	main_gui.o \
	c_$(TARGET)_gui.o


all: cl cgi gui


cl: $(OBJS_CL)
	gcc $(OBJS_CL) -o ../../bin/fox $(LFLAGS) $(COMMON_LFLAGS)


cgi: $(OBJS_CGI)
	gcc $(OBJS_CGI) -o ../../bin/fox.cgi $(LFLAGS) $(COMMON_LFLAGS)


gui: $(OBJS_GUI)
	gcc $(OBJS_GUI) -o $(GUI_DEST) $(LFLAGS) $(COMMON_LFLAGS) $(GUI_LFLAGS)


ifeq ($(TARGET),osx)
c_$(TARGET)_gui.o: c_$(TARGET)_gui.m
	cc -c $< $(CFLAGS) $(COMMON_CFLAGS) $(GUI_CFLAGS)

else
c_$(TARGET)_gui.o: c_$(TARGET)_gui.c
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS) $(GUI_CFLAGS)


endif

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)


clean:
	-rm *.o


