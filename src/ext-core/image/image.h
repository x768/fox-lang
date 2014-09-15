/*
 * image.h
 *
 *  Created on: 2012/06/02
 *      Author: frog
 */

#ifndef IMAGE_H_
#define IMAGE_H_

#include "fox.h"
#include "m_image.h"
#include "m_math.h"


enum {
	QUANT_MODE_RGB,
	QUANT_MODE_RGBA,
	QUANT_MODE_KEY,
};

#ifdef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;

extern RefNode *mod_image;
extern RefNode *cls_color;

#ifdef DEFINE_GLOBALS
#undef extern
#endif


// imgutil.c
int bands_to_channels(int bands);
uint32_t color_hsl_sub(double h, double s, double l);
int image_color_value_to_buf(uint8_t *p, RefImage *img, Value v, int x);
int image_convert_sub(RefImage *dst, int ch_dst, RefImage *src, int ch_src, RefMatrix *mat);
int image_convert_palette(uint32_t *palette, RefMatrix *mat);
int image_convert_to_fullcolor(RefImage *dst, int dst_ch, RefImage *src, RefMatrix *mat);
void copy_image_sub(RefImage *dst, const RefImage *src, int dx, int dy, int32_t *src_rect, int alpha);
void copy_image_resized_sub(RefImage *dst, const RefImage *src, int32_t *dst_rect, int32_t *src_rect, int alpha, int resampled);

// quantize.c
void quantize(RefImage *dst, const RefImage *src, int mode, int dither);
int get_palette_mode(uint32_t *palette);
uint8_t *get_nearest_palette(uint32_t *palette, int mode);
uint8_t *get_convert_palette_table(uint32_t *dst, const uint32_t *src);
void quantize_palette(RefImage *dst, const RefImage *src, int dither);


#endif /* IMAGE_H_ */
