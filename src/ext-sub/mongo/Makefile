include ../../Makefile.env
include ../../Makefile.common

.PHONY: clean

ifeq ($(TARGET),posix)
CFLAGS = -I../../include -fPIC -I../../lib/mongo-c-driver/src/mongoc -I../../lib/mongo-c-driver/libbson/src/bson
LFLAGS = $(COMMON_DLFLAGS) ../../lib/mongo-c-driver/.libs/libmongoc-priv.a ../../lib/mongo-c-driver/libbson/.libs/libbson.a -lssl
endif
ifeq ($(TARGET),win)
CFLAGS = -I../../include -I/usr/local/include -I../../lib/mongo-c-driver/src/mongoc -I../../lib/mongo-c-driver/src/libbson/src/bson
LFLAGS = $(COMMON_DLFLAGS) ../../lib/mongo-c-driver/.libs/libmongoc-priv.a ../../lib/mongo-c-driver/src/libbson/.libs/libbson.a -L../../lib/openssl -lssl -lcrypto -lws2_32
endif
ifeq ($(TARGET),osx)
CFLAGS = -I../../include -I../../lib/mongo-c-driver/src/mongoc -I../../lib/mongo-c-driver/src/libbson/src/bson -I../../lib/libpcre
LFLAGS = $(COMMON_DLFLAGS) ../../lib/mongo-c-driver/.libs/libmongoc-priv.a ../../lib/mongo-c-driver/src/libbson/.libs/libbson.a -lssl -lcrypto
endif

DEST = ../../../import/db/mongo.so
VPATH = ../../common

OBJS = \
	m_mongo.o \
	strutil.o \


all: $(OBJS)
	gcc $(OBJS) -o $(DEST) $(LFLAGS) $(COMMON_LFLAGS)

clean:
	-rm *.o

.c.o:
	gcc -c $< $(CFLAGS) $(COMMON_CFLAGS)
