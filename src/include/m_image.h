#ifndef M_IMAGE_H_INCLUDED
#define M_IMAGE_H_INCLUDED


#include "fox.h"

enum {
    PALETTE_NUM = 256,
    MAX_IMAGE_SIZE = 32767,

    COLOR_R_SHIFT = 0,
    COLOR_G_SHIFT = 8,
    COLOR_B_SHIFT = 16,
    COLOR_A_SHIFT = 24,
    COLOR_R_MASK = 0x000000FF,
    COLOR_G_MASK = 0x0000FF00,
    COLOR_B_MASK = 0x00FF0000,
    COLOR_A_MASK = 0xFF000000,
};
enum {
    BAND_ERROR,
    BAND_L,
    BAND_P,
    BAND_LA,
    BAND_RGB,
    BAND_RGBA,
};
enum {
    INDEX_RECT_X,
    INDEX_RECT_Y,
    INDEX_RECT_W,
    INDEX_RECT_H,
    INDEX_RECT_NUM,
};

typedef struct {
    RefHeader rh;

    int bands;
    uint32_t *palette;
    int32_t width, height;
    int32_t pitch;
    uint8_t *data;
} RefImage;

typedef struct {
    RefHeader rh;
    int32_t i[INDEX_RECT_NUM];
} RefRect;

typedef struct {
    int sc, dc;
    int mode;
    int src_mode;
    uint8_t *pal_conv;
    uint8_t *pal_conv_rgba;
    uint32_t pal_alpha[256 / 32];
} CopyParam;

typedef struct {
    void (*CopyParam_init)(CopyParam *cp, RefImage *dst, const RefImage *src, int alpha, int resampled);
    void (*copy_image_line)(uint8_t *pd, RefImage *dst, const uint8_t *ps, const RefImage *src, int w, const CopyParam *cp, int alpha);
} ImageStatic;

#endif /* M_IMAGE_H_INCLUDED */
