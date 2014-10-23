#ifndef _M_MEDIA_H_
#define _M_MEDIA_H_

#include "fox.h"

typedef struct {
    RefHeader rh;

    int samples;  // samples per sec
    int width;    // 1:8bit  2:16bit
    int channels;
    int length;   // length by samples (not bytes)
    int max;      // 可変長バッファに実際に確保されているバイト数
    union {
        uint8_t *u8;
        int16_t *i16;
    } u;
} RefAudio;

#endif /* _M_MEDIA_H_ */
