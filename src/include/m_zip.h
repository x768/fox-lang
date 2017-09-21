#ifndef M_ZIP_H_INCLUDED
#define M_ZIP_H_INCLUDED

#include "fox.h"


typedef struct {
    const char *path;
    int32_t offset;
    int32_t size;
} ZipEntry;


typedef struct {
    Hash *(*get_entry_map_static)(const char *path, Mem *mem);
    int (*read_entry)(char *buf, const ZipEntry *entry);
} ZipStatic;


#endif /* M_ZIP_H_INCLUDED */
