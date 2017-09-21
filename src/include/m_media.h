#ifndef M_MEDIA_H_INCLUDED
#define M_MEDIA_H_INCLUDED

#include "fox.h"

typedef struct {
    RefHeader rh;

    int samples;    // samples per sec
    int width;      // 1:8bit  2:16bit
    int channels;
    int length;     // length by samples (not bytes)
    int alloc_size; // 可変長バッファに実際に確保されているバイト数
    union {
        uint8_t *u8;
        int16_t *i16;
    } u;
} RefAudio;

#endif /* M_MEDIA_H_INCLUDED */
