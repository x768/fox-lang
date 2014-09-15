/*
 * imgutil.c
 *
 *  Created on: 2012/07/15
 *      Author: frog
 */

#include "image.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


typedef struct {
	int sc, dc;
	int mode;
	int src_mode;
	uint8_t *pal_conv;
	uint8_t *pal_conv_rgba;
	uint32_t pal_alpha[256 / 32];
} CopyParam;

int bands_to_channels(int bands)
{
	switch (bands) {
	case BAND_LA:
		return 2;
	case BAND_RGB:
		return 3;
	case BAND_RGBA:
		return 4;
	default:
		return 1;
	}
}
uint32_t color_hsl_sub(double h, double s, double l)
{
	uint32_t ret;

	if (h >= 360.0) {
		h = fmod(h, 360.0);
	} else if (h < 0.0) {
		h = fmod(h, 360.0);
		if (h < 0.0) {
			h = 360 + h;
		}
	}
	if (s < 0.0) {
		s = 0.0;
	} else if (s > 1.0) {
		s = 1.0;
	}
	if (l < 0.0) {
		l = 0.0;
	} else if (l > 1.0) {
		l = 1.0;
	}

	{
		int ih = (((int)h) / 60);
		double f = h / 60.0 - (double)ih;
		double g = 1.0 - f;

		double hi = 0.5 + 0.5 * s;
		double lo = 0.5 - 0.5 * s;
		double up = f * s + 0.5 * (1.0 - s);
		double dw = g * s + 0.5 * (1.0 - s);

		int ihi, ilo, iup, idw;

		if (l < 0.5) {
			double m = l * 2.0;
			hi = hi * m;
			lo = lo * m;
			up = up * m;
			dw = dw * m;
		} else if (l > 0.5) {
			double m = (1.0 - l) * 2.0;
			hi = 1.0 - (1.0 - hi) * m;
			lo = 1.0 - (1.0 - lo) * m;
			up = 1.0 - (1.0 - up) * m;
			dw = 1.0 - (1.0 - dw) * m;
		}

		ihi = (int)(hi * 255.0);
		ilo = (int)(lo * 255.0);
		iup = (int)(up * 255.0);
		idw = (int)(dw * 255.0);

		switch (ih) {
		case 0: case 6:
			ret = ihi | (iup << 8) | (ilo << 16);
			break;
		case 1:
			ret = idw | (ihi << 8) | (ilo << 16);
			break;
		case 2:
			ret = ilo | (ihi << 8) | (iup << 16);
			break;
		case 3:
			ret = ilo | (idw << 8) | (ihi << 16);
			break;
		case 4:
			ret = iup | (ilo << 8) | (ihi << 16);
			break;
		case 5:
			ret = ihi | (ilo << 8) | (idw << 16);
			break;
		default:
			// Cannot happen
			ret = 0;
			break;
		}
	}
	return ret;
}

static void get_convert_matrix(int ch_src, int ch_dst, int (*mx)[4], int *off)
{
	// -  1  2  3  4
	// 1  T  M  L  L
	// 2  M  T  L  L
	// 3  C  C  T  K
	// 4  C  C  K  T
	memset(off, 0, sizeof(int) * 4);
	memset(mx, 0, sizeof(int) * 4 * 4);

	if (ch_src == ch_dst) {
		// T
		int x;
		for (x = 0; x < 4; x++) {
			mx[x][x] = 256;
		}
	} else if (ch_src <= 2 && ch_dst <= 2) {
		// M
		mx[0][0] = 256;
		off[1] = 65536;
	} else if (ch_src >= 3 && ch_dst >= 3) {
		// K
		int x;
		for (x = 0; x < 3; x++) {
			mx[x][x] = 256;
		}
		off[3] = 63356;
	} else if (ch_src <= 2) {
		// C
		int x;
		for (x = 0; x < 3; x++) {
			mx[0][x] = 256;
		}
		if (ch_src == 1) {
			off[3] = 65536;
		} else {
			mx[1][3] = 256;
		}
	} else if (ch_dst <= 2) {
		// L
		mx[0][0] = 77;
		mx[1][0] = 150;
		mx[2][0] = 29;
		off[1] = 65536;
	}
}
int image_convert_sub(RefImage *dst, int ch_dst, RefImage *src, int ch_src, RefMatrix *mat)
{
	int width = src->width;
	int height = src->height;
	uint8_t *src_p = src->data;
	uint8_t *dst_p;
	int mx[4][4];
	int off[4];
	int x, y;

	if (mat != NULL) {
		if (mat->cols != ch_src + 1) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "RefMatrix has %d columns (%d expected)", mat->cols, ch_src + 1);
			return FALSE;
		}
		if (mat->rows != ch_dst) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "RefMatrix has %d rows (%d expected)", mat->rows, ch_dst);
			return FALSE;
		}
		for (y = 0; y < ch_dst; y++) {
			for (x = 0; x < ch_src; x++) {
				mx[x][y] = (int)(mat->d[y * mat->cols + x] * 256.0);
			}
		}
		for (y = 0; y < ch_dst; y++) {
			off[y] = (int)(mat->d[y * mat->cols + ch_dst] * 65536.0);
		}
	} else {
		get_convert_matrix(ch_src, ch_dst, mx, off);
	}

	dst_p = dst->data;

	for (y = 0; y < height; y++) {
		const uint8_t *src_line = src_p + src->pitch * y;
		uint8_t *dst_line = dst_p + dst->pitch * y;
		for (x = 0; x < width; x++) {
			int i, j;
			for (i = 0; i < ch_dst; i++) {
				int sum = off[i];
				for (j = 0; j < ch_src; j++) {
					sum += src_line[x * ch_src + j] * mx[j][i];
				}
				sum >>= 8;
				if (sum < 0) {
					sum = 0;
				} else if (sum > 255) {
					sum = 255;
				}
				dst_line[x * ch_dst + i] = sum;
			}
		}
	}

	return TRUE;
}

int image_convert_palette(uint32_t *palette, RefMatrix *mat)
{
	int mx[4][4];
	int off[4];
	int x, y;

	if (mat->cols != 4 && mat->cols != 5) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "RefMatrix has %d columns (4 or 5 expected)", mat->cols);
		return FALSE;
	}
	if (mat->rows != 3 && mat->rows != 4) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "RefMatrix has %d rows (3 or 4 expected)", mat->rows);
		return FALSE;
	}

	memset(mx, 0, sizeof(mx));
	for (y = 0; y < mat->rows; y++) {
		for (x = 0; x < mat->cols - 1; x++) {
			mx[x][y] = (int)(mat->d[y * mat->cols + x] * 256.0);
		}
	}
	for (y = 0; y < mat->rows; y++) {
		// 一番右の列
		off[y] = (int)(mat->d[(y + 1) * mat->cols - 1] * 65536.0);
	}
	if (mat->rows == 3) {
		off[3] = 65536;
	}

	for (x = 0; x < PALETTE_NUM; x++) {
		int i, j;
		int tmp = 0;
		int c = palette[x];

		for (i = 0; i < 4; i++) {
			int sum = off[i];
			for (j = 0; j < 4; j++) {
				sum += ((c >> (j * 8)) & 0xFF) * mx[j][i];
			}
			sum >>= 8;
			if (sum < 0) {
				sum = 0;
			} else if (sum > 255) {
				sum = 255;
			}
			tmp |= sum << (i * 8);
		}
		palette[x] = tmp;
	}
	return TRUE;
}
int image_color_value_to_buf(uint8_t *p, RefImage *img, Value v, int x)
{
	uint32_t color = Value_integral(v);
	const RefNode *type = fs->Value_type(v);

	if (type == cls_color) {
		uint8_t r = (color & COLOR_R_MASK) >> COLOR_R_SHIFT;
		uint8_t g = (color & COLOR_G_MASK) >> COLOR_G_SHIFT;
		uint8_t b = (color & COLOR_B_MASK) >> COLOR_B_SHIFT;
		uint8_t a = (color & COLOR_A_MASK) >> COLOR_A_SHIFT;
		switch (img->bands) {
		case BAND_L:
			p[x] = (r * 77 + g * 150 + b * 29) / 256;
			break;
		case BAND_LA:
			p[x * 2 + 0] = (r * 77 + g * 150 + b * 29) / 256;
			p[x * 2 + 1] = a;
			break;
		case BAND_RGB:
			p[x * 3 + 0] = r;
			p[x * 3 + 1] = g;
			p[x * 3 + 2] = b;
			break;
		case BAND_RGBA:
			p[x * 4 + 0] = r;
			p[x * 4 + 1] = g;
			p[x * 4 + 2] = b;
			p[x * 4 + 3] = a;
			break;
		case BAND_P:
			goto TYPE_ERROR;
		}
	} else if (type == fs->cls_int) {
		switch (img->bands) {
		case BAND_L:
			p[x] = color;
			break;
		case BAND_LA:
			p[x * 2 + 0] = color >> 8;
			p[x * 2 + 1] = color;
			break;
		case BAND_RGB:
			p[x * 3 + 0] = color >> 16;
			p[x * 3 + 1] = color >> 8;
			p[x * 3 + 2] = color;
			break;
		case BAND_RGBA:
			p[x * 4 + 0] = color >> 24;
			p[x * 4 + 1] = color >> 16;
			p[x * 4 + 2] = color >> 8;
			p[x * 4 + 3] = color;
			break;
		case BAND_P:
			// TODO 一番近い色を探す
			p[x] = color;
			break;
		}
	} else {
		goto TYPE_ERROR;
	}
	return TRUE;

TYPE_ERROR:
	fs->throw_errorf(fs->mod_lang, "TypeError", "Int or Color required but %n", type);
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////

/*
 * パレットインデックスをL,LA,RGB,RGBAに変換する
 * matが指定されていた場合、変換行列をかける
 */
int image_convert_to_fullcolor(RefImage *dst, int ch_dst, RefImage *src, RefMatrix *mat)
{
	int mx[4][4];
	int off[4];
	int x, y;
	const uint32_t *pal = src->palette;

	if (mat != NULL) {
		if (mat->cols != 4 && mat->cols != 5) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "RefMatrix has %d columns (4 or 5 expected)", mat->cols);
			return FALSE;
		}
		if (mat->rows != ch_dst) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "RefMatrix has %d rows (%d expected)", mat->rows, ch_dst);
			return FALSE;
		}
		memset(mx, 0, sizeof(mx));
		for (y = 0; y < mat->rows; y++) {
			for (x = 0; x < mat->cols - 1; x++) {
				mx[x][y] = (int)(mat->d[y * mat->cols + x] * 256.0);
			}
		}
		for (y = 0; y < mat->rows; y++) {
			// 一番右の列
			off[y] = (int)(mat->d[(y + 1) * mat->cols - 1] * 65536.0);
		}
	}

	for (y = 0; y < dst->height; y++) {
		const uint8_t *sp = src->data + y * src->pitch;
		uint8_t *dp = dst->data + y * dst->pitch;

		if (mat != NULL) {
			for (x = 0; x < dst->width; x++) {
				uint32_t c = pal[sp[x]];
				int i;
				for (i = 0; i < ch_dst; i++) {
					int sum = off[i];
					int j;
					for (j = 0; j < 4; j++) {
						sum += ((c >> (j * 8)) & 0xFF) * mx[j][i];
					}
					sum >>= 8;
					if (sum < 0) {
						sum = 0;
					} else if (sum > 255) {
						sum = 255;
					}
					dp[x * ch_dst + i] = sum;
				}
			}
		} else {
			for (x = 0; x < dst->width; x++) {
				uint32_t c = pal[sp[x]];
				int i;
				for (i = 0; i < ch_dst; i++) {
					dp[x * ch_dst + i] = (c >> (i * 8)) & 0xFF;
				}
			}
		}
	}

	return TRUE;
}

static void CopyParam_init(CopyParam *cp, RefImage *dst, const RefImage *src, int alpha, int resampled)
{
	if (dst->bands == BAND_P) {
		if (src->bands == BAND_P && !resampled) {
			cp->pal_conv = get_convert_palette_table(dst->palette, src->palette);
			cp->pal_conv_rgba = NULL;
		} else {
			cp->mode = get_palette_mode(dst->palette);
			cp->pal_conv_rgba = get_nearest_palette(dst->palette, cp->mode);
			cp->pal_conv = NULL;
		}
	} else {
		cp->pal_conv = NULL;
		cp->pal_conv_rgba = NULL;
	}
	cp->dc = bands_to_channels(dst->bands);

	if (dst->bands == BAND_P && src->bands == BAND_P) {
		cp->pal_conv = get_convert_palette_table(dst->palette, src->palette);
	} else {
		cp->pal_conv = NULL;
	}
	if (src->bands == BAND_P && alpha) {
		int i;
		memset(cp->pal_alpha, 0, sizeof(cp->pal_alpha));
		for (i = 0; i < PALETTE_NUM; i++) {
			int a = (src->palette[i] & COLOR_A_MASK) >> COLOR_A_SHIFT;
			if (a >= 64) {
				cp->pal_alpha[i / 32] |= 1 << (i % 32);
			}
		}
	}
	if (src->bands == BAND_P && resampled) {
		cp->src_mode = BAND_RGBA;
		cp->sc = 4;
	} else {
		cp->src_mode = src->bands;
		cp->sc = bands_to_channels(src->bands);
	}
}
/**
 * 1行だけ転送する
 * フォーマットが異なる場合、変換する
 * alpha : TRUEならアルファチャンネルを考慮する
 *         ただし、転送先がパレットモードの場合、透明か不透明かだけ判断
 * TODO: パレットモード以外からパレットモードへの変換
 */
static void copy_image_line(uint8_t *pd, RefImage *dst, const uint8_t *ps, const RefImage *src, int w, const CopyParam *cp, int alpha)
{
	int x;

	switch (dst->bands) {
	case BAND_P:
		switch (cp->src_mode) {
		case BAND_L:
			if (cp->mode == QUANT_MODE_RGBA) {
				for (x = 0; x < w; x++) {
					uint32_t g = *ps++;
					*pd++ = cp->pal_conv_rgba[g | (g << 5) | (g << 10) | (7 << 15)];
				}
			} else {
				for (x = 0; x < w; x++) {
					uint32_t g = *ps++;
					*pd++ = cp->pal_conv_rgba[g | (g << 5) | (g << 10)];
				}
			}
			break;
		case BAND_P:
			if (alpha) {
				for (x = 0; x < w; x++) {
					int idx = *ps;
					if ((cp->pal_alpha[idx / 32] & (1 << (idx % 32))) != 0) {
						*pd = cp->pal_conv[*ps];
					}
					pd++; ps++;
				}
			} else {
				for (x = 0; x < w; x++) {
					*pd++ = cp->pal_conv[*ps];
					ps++;
				}
			}
			break;
		case BAND_RGB:
			if (cp->mode == QUANT_MODE_RGBA) {
				for (x = 0; x < w; x++) {
					uint32_t r = ps[0] >> 3;
					uint32_t g = ps[1] >> 3;
					uint32_t b = ps[2] >> 3;

					*pd++ = cp->pal_conv_rgba[r | (g << 5) | (b << 10) | (7 << 15)];
					ps += 3;
				}
			} else {
				for (x = 0; x < w; x++) {
					uint32_t r = ps[0] >> 3;
					uint32_t g = ps[1] >> 3;
					uint32_t b = ps[2] >> 3;

					*pd++ = cp->pal_conv_rgba[r | (g << 5) | (b << 10)];
					ps += 3;
				}
			}
			break;
		case BAND_RGBA:
			if (cp->mode == QUANT_MODE_RGBA) {
				for (x = 0; x < w; x++) {
					uint32_t a = ps[3];

					if (!alpha || a >= 64) {
						uint32_t r = ps[0] >> 3;
						uint32_t g = ps[1] >> 3;
						uint32_t b = ps[2] >> 3;
						a >>= 5;
						*pd++ = cp->pal_conv_rgba[r | (g << 5) | (b << 10) | (a << 15)];
					} else {
						pd++;
					}
					ps += 4;
				}
			} else {
				for (x = 0; x < w; x++) {
					uint32_t a = ps[3];

					if (!alpha || a >= 64) {
						uint32_t r = ps[0] >> 3;
						uint32_t g = ps[1] >> 3;
						uint32_t b = ps[2] >> 3;
						*pd++ = cp->pal_conv_rgba[r | (g << 5) | (b << 10)];
					} else {
						pd++;
					}
					ps += 4;
				}
			}
			break;
		}
		break;
	case BAND_L:
		switch (cp->src_mode) {
		case BAND_L:
			memcpy(pd, ps, w * cp->dc);
			break;
		case BAND_LA:
			if (alpha) {
				for (x = 0; x < w; x++) {
					int a1 = ps[1];
					int a2 = 255 - a1;
					*pd = (*ps * a1 + *pd * a2) / 255;
					pd++; ps++;
					ps++;
				}
			} else {
				for (x = 0; x < w; x++) {
					*pd++ = *ps++;
					ps++;
				}
			}
			break;
		}
		break;
	case BAND_LA:
		switch (cp->src_mode) {
		case BAND_L:
			for (x = 0; x < w; x++) {
				*pd++ = *ps++;
				*pd++ = 255;
			}
			break;
		case BAND_LA:
			if (alpha) {
				for (x = 0; x < w; x++) {
					int a1 = ps[1];
					if (a1 > 0) {
						int a2 = ((255 - a1) * pd[1]) / 255;
						int a = a1 + a2;
						*pd = (*ps * a1 + *pd * a2) / a;
						pd++; ps++;
						*pd = *pd + ((255 - *pd) * *ps) / 255;
						pd++; ps++;
					} else {
						pd += 2;
						ps += 2;
					}
				}
			} else {
				memcpy(pd, ps, w * cp->dc);
			}
			break;
		}
		break;
	case BAND_RGB:
		switch (cp->src_mode) {
		case BAND_RGBA:
			if (alpha) {
				for (x = 0; x < w; x++) {
					int a1 = ps[3];
					int a2 = 255 - a1;
					*pd = (*ps * a1 + *pd * a2) / 255; // R
					pd++; ps++;
					*pd = (*ps * a1 + *pd * a2) / 255; // G
					pd++; ps++;
					*pd = (*ps * a1 + *pd * a2) / 255; // B
					pd++; ps++;
					ps++;
				}
			} else {
				for (x = 0; x < w; x++) {
					*pd++ = *ps++; // R
					*pd++ = *ps++; // G
					*pd++ = *ps++; // B
					ps++;
				}
			}
			break;
		case BAND_RGB:
			memcpy(pd, ps, w * cp->dc);
			break;
		case BAND_L:
			for (x = 0; x < w; x++) {
				*pd++ = *ps; // R
				*pd++ = *ps; // G
				*pd++ = *ps; // B
				ps++;
			}
			break;
		case BAND_LA:
			if (alpha) {
				for (x = 0; x < w; x++) {
					int a1 = ps[3];
					int a2 = 255 - a1;
					*pd = (*ps * a1 + *pd * a2) / 255; // R
					pd++;
					*pd = (*ps * a1 + *pd * a2) / 255; // G
					pd++;
					*pd = (*ps * a1 + *pd * a2) / 255; // B
					pd++;
					ps++;
				}
			} else {
				for (x = 0; x < w; x++) {
					*pd++ = *ps; // R
					*pd++ = *ps; // G
					*pd++ = *ps; // B
					ps++; ps++;
				}
			}
			break;
		case BAND_P: {
			const uint32_t *pal = src->palette;
			if (alpha) {
				for (x = 0; x < w; x++) {
					uint32_t c = pal[*ps];
					int a1 = (c & COLOR_A_MASK) >> COLOR_A_SHIFT;
					int a2 = 255 - a1;
					int r = (c & COLOR_R_MASK) >> COLOR_R_SHIFT;
					int g = (c & COLOR_G_MASK) >> COLOR_G_SHIFT;
					int b = (c & COLOR_B_MASK) >> COLOR_B_SHIFT;
					*pd = (r * a1 + *pd * a2) / 255; // R
					pd++;
					*pd = (g * a1 + *pd * a2) / 255; // G
					pd++;
					*pd = (b * a1 + *pd * a2) / 255; // B
					pd++;
					ps++;
				}
			} else {
				for (x = 0; x < w; x++) {
					uint32_t c = pal[*ps];
					*pd++ = (c & COLOR_R_MASK) >> COLOR_R_SHIFT;
					*pd++ = (c & COLOR_G_MASK) >> COLOR_G_SHIFT;
					*pd++ = (c & COLOR_B_MASK) >> COLOR_B_SHIFT;
					ps++;
				}
			}
			break;
		}
		}
		break;
	case BAND_RGBA:
		switch (cp->src_mode) {
		case BAND_RGBA:
			if (alpha) {
				for (x = 0; x < w; x++) {
					int a1 = ps[3];
					if (a1 > 0) {
						int a2 = ((255 - a1) * pd[3]) / 255;
						int a = a1 + a2;
						*pd = (*ps * a1 + *pd * a2) / a; // R
						pd++; ps++;
						*pd = (*ps * a1 + *pd * a2) / a; // G
						pd++; ps++;
						*pd = (*ps * a1 + *pd * a2) / a; // B
						pd++; ps++;
						*pd = *pd + ((255 - *pd) * *ps) / 255;
						pd++; ps++;
					} else {
						pd += 4;
						ps += 4;
					}
				}
			} else {
				memcpy(pd, ps, w * cp->dc);
			}
			break;
		case BAND_RGB:
			for (x = 0; x < w; x++) {
				*pd++ = *ps++;
				*pd++ = *ps++;
				*pd++ = *ps++;
				*pd++ = 255;
			}
			break;
		case BAND_L:
			for (x = 0; x < w; x++) {
				*pd++ = *ps;
				*pd++ = *ps;
				*pd++ = *ps;
				ps++;
				*pd++ = 255;
			}
			break;
		case BAND_LA:
			if (alpha) {
				for (x = 0; x < w; x++) {
					int a1 = ps[1];
					if (a1 > 0) {
						int a2 = ((255 - a1) * pd[3]) / 255;
						int a = a1 + a2;
						*pd = (*ps * a1 + *pd * a2) / a; // R
						pd++;
						*pd = (*ps * a1 + *pd * a2) / a; // G
						pd++;
						*pd = (*ps * a1 + *pd * a2) / a; // B
						pd++;
						*pd = *pd + ((255 - *pd) * *ps) / 255;
						pd++; ps++;
					} else {
						pd += 4;
						ps += 2;
					}
				}
			} else {
				for (x = 0; x < w; x++) {
					*pd++ = *ps;
					*pd++ = *ps;
					*pd++ = *ps;
					ps++;
					*pd++ = *ps++;
				}
			}
			break;
		case BAND_P: {
			const uint32_t *pal = src->palette;
			if (alpha) {
				for (x = 0; x < w; x++) {
					uint32_t c = pal[*ps];
					int a1 = (c & COLOR_A_MASK) >> COLOR_A_SHIFT;
					if (a1 > 0) {
						int a2 = 255 - a1;
						int a = a1 + a2;
						int r = (c & COLOR_R_MASK) >> COLOR_R_SHIFT;
						int g = (c & COLOR_G_MASK) >> COLOR_G_SHIFT;
						int b = (c & COLOR_B_MASK) >> COLOR_B_SHIFT;
						*pd = (r * a1 + *pd * a2) / a; // R
						pd++;
						*pd = (g * a1 + *pd * a2) / a; // G
						pd++;
						*pd = (b * a1 + *pd * a2) / a; // B
						pd++;
						*pd = *pd + ((255 - *pd) * a) / 255;
						pd++;
						ps++;
					} else {
						pd += 4;
						ps += 1;
					}
				}
			} else {
				for (x = 0; x < w; x++) {
					uint32_t c = pal[*ps];
					*pd++ = (c & COLOR_R_MASK) >> COLOR_R_SHIFT;
					*pd++ = (c & COLOR_G_MASK) >> COLOR_G_SHIFT;
					*pd++ = (c & COLOR_B_MASK) >> COLOR_B_SHIFT;
					*pd++ = (c & COLOR_A_MASK) >> COLOR_A_SHIFT;
					ps++;
				}
			}
			break;
		}
		}
		break;
	}
}
/**
 * トリミングを行う
 * フォーマットが異なる場合、変換する
 */
void copy_image_sub(RefImage *dst, const RefImage *src, int dx, int dy, int32_t *src_rect, int alpha)
{
	int sx = src_rect[0];
	int sy = src_rect[1];
	int w = src_rect[2];
	int h = src_rect[3];
	int dc = bands_to_channels(dst->bands);
	int sc = bands_to_channels(src->bands);
	int y;
	CopyParam cp;

	CopyParam_init(&cp, dst, src, alpha, FALSE);
	cp.dc = dc;

	// クリッピング
	if (sx < 0) {
		w += sx;
		dx -= sx;
		sx = 0;
	}
	if (sy < 0) {
		h += sy;
		dy -= sy;
		sy = 0;
	}
	if (dx < 0) {
		w += dx;
		sx -= dx;
		dx = 0;
	}
	if (dy < 0) {
		h += dy;
		sy -= dy;
		dy = 0;
	}
	if (w > src->width - sx) {
		w = src->width - sx;
	}
	if (h > src->height - sy) {
		h = src->height - sy;
	}
	if (w > dst->width - dx) {
		w = dst->width - dx;
	}
	if (h > dst->height - dy) {
		h = dst->height - dy;
	}
	if (w <= 0 || h <= 0) {
		free(cp.pal_conv);
		return;
	}

	for (y = 0; y < h; y++) {
		uint8_t *pd = dst->data + (dy + y) * dst->pitch + dx * dc;
		const uint8_t *ps = src->data + (sy + y) * src->pitch + sx * sc;
		copy_image_line(pd, dst, ps, src, w, &cp, alpha);
	}
	free(cp.pal_conv);
	free(cp.pal_conv_rgba);
}
/**
 * 拡大縮小コピー
 * resampled: 線形補完
 */
void copy_image_resized_sub(RefImage *dst, const RefImage *src, int32_t *dst_rect, int32_t *src_rect, int alpha, int resampled)
{
	int sx = src_rect[0];
	int sy = src_rect[1];
	int sw = src_rect[2];
	int sh = src_rect[3];

	int dx = dst_rect[0];
	int dy = dst_rect[1];
	int dw = dst_rect[2];
	int dh = dst_rect[3];

	double fw, fh;  // dst / src

	int x, y;
	CopyParam cp;
	int sc, dc;
	uint8_t *line;

	if (sw <= 0 || sh <= 0) {
		return;
	}
	fw = (double)dw / (double)sw;
	fh = (double)dh / (double)sh;

	CopyParam_init(&cp, dst, src, alpha, resampled);
	sc = cp.sc;
	dc = cp.dc;

	// クリッピング
	if (sx < 0) {
		int ddx = (int)((double)sx * fw);
		dw += ddx;
		dx -= ddx;
		sw += sx;
		sx = 0;
	}
	if (sy < 0) {
		int ddy = (int)((double)sy * fh);
		dh += ddy;
		dx -= ddy;
		sh += sy;
		sy = 0;
	}
	if (dx < 0) {
		int sdx = (int)((double)dx / fw);
		sw += sdx;
		sx -= sdx;
		dx = 0;
	}
	if (dy < 0) {
		int sdy = (int)((double)dy / fh);
		sh += sdy;
		sy -= sdy;
		dy = 0;
	}
	if (sw > src->width - sx) {
		int sdx = sw - (src->width - sx);
		int ddx = (int)((double)sdx / fw);
		sw -= sdx;
		dw -= ddx;
	}
	if (sh > src->height - sy) {
		int sdy = sh - (src->height - sy);
		int ddy = (int)((double)sdy / fh);
		sh -= sdy;
		dh -= ddy;
	}
	if (dw > dst->width - dx) {
		dw = dst->width - dx;
	}
	if (dh > dst->height - dy) {
		dh = dst->height - dy;
	}

	if (dw <= 0 || dh <= 0) {
		free(cp.pal_conv);
		return;
	}
	line = malloc(dw * sc);

	if (resampled) {
		int w2 = sw * sc;
		uint8_t *pd;
		uint8_t *line2 = malloc(w2);

		for (y = 0; y < dh; y++) {
			{
				// y方向にリサイズする
				double ry1 = (double)y / fh;
				double ry2 = (double)(y + 1) / fh;
				int y1 = (int)ry1 + 1;
				int y2 = (int)ry2;
				double pt1 = (double)y1 - ry1;
				double pt2 = ry2 - (double)y2;
				double factor;

				if (y1 <= y2) {
					factor = pt1 + pt2 + (double)(y2 - y1);
					factor = 1.0 / factor;
				} else {
					pt1 = 1.0;
					pt2 = 0.0;
					factor = 1.0;
				}
				y1--;

				if (src->bands == BAND_P) {
					int yy;
					const uint32_t *pal = src->palette;

					for (x = 0; x < w2; x++) {
						int x_4 = x / 4;
						int shift = (x % 4) * 8;
						double sum;
						int isum;

						{
							const uint8_t *ps = src->data + (sy + y1) * src->pitch + sx * sc;
							sum = (double)((pal[ps[x_4]] >> shift) & 0xFF) * pt1;
						}
						for (yy = y1 + 1; yy < y2; yy++) {
							const uint8_t *ps = src->data + (sy + yy) * src->pitch + sx * sc;
							sum += (double)((pal[ps[x_4]] >> shift) & 0xFF);
						}
						if (pt2 > 0.0) {
							const uint8_t *ps = src->data + (sy + yy) * src->pitch + sx * sc;
							sum += (double)((pal[ps[x_4]] >> shift) & 0xFF) * pt2;
						}
						isum = (int)(sum * factor + 0.5);
						line2[x] = (isum < 255 ? isum : 255);
					}
				} else {
					int yy;
					for (x = 0; x < w2; x++) {
						double sum;
						int isum;
						{
							const uint8_t *ps = src->data + (sy + y1) * src->pitch + sx * sc;
							sum = (double)ps[x] * pt1;
						}
						for (yy = y1 + 1; yy < y2; yy++) {
							const uint8_t *ps = src->data + (sy + yy) * src->pitch + sx * sc;
							sum += (double)ps[x];
						}
						if (pt2 > 0.0) {
							const uint8_t *ps = src->data + (sy + yy) * src->pitch + sx * sc;
							sum += (double)ps[x] * pt2;
						}
						isum = (int)(sum * factor + 0.5);
						line2[x] = (isum < 255 ? isum : 255);
					}
				}
			}
			// x方向にリサイズする
			pd = line;
			for (x = 0; x < dw; x++) {
				double rx1 = (double)x / fw;
				double rx2 = (double)(x + 1) / fw;
				int x1 = (int)rx1 + 1;
				int x2 = (int)rx2;
				double pt1 = (double)x1 - rx1;
				double pt2 = rx2 - (double)x2;
				double factor;
				int i;

				if (x1 <= x2) {
					factor = pt1 + pt2 + (double)(x2 - x1);
					factor = 1.0 / factor;
				} else {
					pt1 = 1.0;
					pt2 = 0.0;
					factor = 1.0;
				}
				x1--;

				for (i = 0; i < sc; i++) {
					int xx;
					double sum;
					int isum;
					const uint8_t *ps = line2 + i;
					pd = line + i;

					sum = (double)ps[x1 * sc] * pt1;
					for (xx = x1 + 1; xx < x2; xx++) {
						sum += (double)ps[xx * sc];
					}
					if (pt2 > 0.0) {
						sum += (double)ps[xx * sc] * pt2;
					}
					isum = (int)(sum * factor + 0.5);
					pd[x * sc] = (isum < 255 ? isum : 255);
				}
			} // for x

			pd = dst->data + (dy + y) * dst->pitch + dx * dc;
			copy_image_line(pd, dst, line, src, dw, &cp, alpha);
		}
		free(line2);
	} else {
		// 最近傍法
		for (y = 0; y < dh; y++) {
			uint8_t *pd = dst->data + (dy + y) * dst->pitch + dx * dc;
			uint8_t *pl = line;
			const uint8_t *ps = src->data + (sy + (int)((double)y / fh)) * src->pitch + sx * sc;

			switch (sc) {
			case 1:
				for (x = 0; x < dw; x++) {
					int x2 = (int)((double)x / fw);
					*pl++ = ps[x2];
				}
				break;
			case 2:
				for (x = 0; x < dw; x++) {
					int x2 = (int)((double)x / fw);
					const uint8_t *p = &ps[x2 * 2];
					*pl++ = p[0];
					*pl++ = p[1];
				}
				break;
			case 3:
				for (x = 0; x < dw; x++) {
					const uint8_t *p = &ps[((int)((double)x / fw)) * 3];
					*pl++ = p[0];
					*pl++ = p[1];
					*pl++ = p[2];
				}
				break;
			case 4:
				for (x = 0; x < dw; x++) {
					const uint8_t *p = &ps[((int)((double)x / fw)) * 4];
					*pl++ = p[0];
					*pl++ = p[1];
					*pl++ = p[2];
					*pl++ = p[3];
				}
				break;
			}
			copy_image_line(pd, dst, line, src, dw, &cp, alpha);
		}
	}
	free(line);
	free(cp.pal_conv);
	free(cp.pal_conv_rgba);
}
