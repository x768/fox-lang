#ifndef FOX_IO_H_INCLUDED
#define FOX_IO_H_INCLUDED

#include "fox.h"
#include "compat.h"
#include <stdio.h>

// class StreamIO
enum {
    INDEX_READ_CUR,
    INDEX_READ_MAX,
    INDEX_READ_MEMIO,
    INDEX_READ_OFFSET,  // キャッシュ先頭のファイル先頭からのオフセット (int62)

    INDEX_WRITE_MAX,
    INDEX_WRITE_MEMIO,
    INDEX_WRITE_BUFFERING,

    INDEX_STREAM_NUM,
};
// class FileIO
enum {
    INDEX_FILEIO_HANDLE = INDEX_STREAM_NUM,
    INDEX_FILEIO_NUM,
};

enum {
    STREAM_READ = 1,
    STREAM_WRITE = 2,
};

////////////////////////////////////////////////////////////////////////////////

typedef struct {
    RefHeader rh;

    FileHandle fd_read;
    FileHandle fd_write;
} RefFileHandle;

typedef struct {
    RefHeader rh;

    Value dummy[INDEX_STREAM_NUM];
    int cur;
    StrBuf buf;
} RefBytesIO;

#endif /* FOX_IO_H_INCLUDED */
