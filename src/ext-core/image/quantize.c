/*
 * quantize.c
 *
 *  Created on: 2012/07/21
 *      Author: frog
 */

#include "image.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum {
	RGB_MAX = 31,
	A_MAX = 7,
	RGB_SHIFT = 3,
	A_SHIFT = 5,

	HIST_SIZE_RGB = 32 * 32 * 32,
	HIST_SIZE_RGBA = 32 * 32 * 32 * 8,
	TRANSPARENT_THRES = 64 >> A_SHIFT,
};

typedef struct {
	int min[4];     // 最小値
	int max[4];     // 最大値+1
	double var[4];  // 分散
	int ave[4];     // 平均
	int med[4];     // 中央値
	int count;
} QuantColor;

// RGBモードの場合は、v[3]は常に0
#define RGBA_TO_INDEX(v) ((v)[0] | ((v)[1] << 5) | ((v)[2] << 10) | ((v)[3] << 15))


static void sample_color_distribution(uint32_t *dist, QuantColor *qc, const RefImage *src, int mode)
{
	int x, y;
	int width = src->width;
	int height = src->height;
	int has_alpha = (src->bands == BAND_RGBA);
	int channels = (has_alpha ? 4 : 3);

	qc->count = 0;

	for (y = 0; y < height; y++) {
		const uint8_t *p = src->data + src->pitch * y;
		for (x = 0; x < width; x++) {
			int idx;
			int v[4];

			v[0] = p[0] >> RGB_SHIFT;
			v[1] = p[1] >> RGB_SHIFT;
			v[2] = p[2] >> RGB_SHIFT;
			if (mode == QUANT_MODE_RGB) {
				// RGBモードの場合は、v[3]は常に0
				v[3] = 0;
			} else {
				if (has_alpha) {
					v[3] = p[3] >> A_SHIFT;
				} else {
					v[3] = A_MAX;
				}
			}
			p += channels;

			if (mode == QUANT_MODE_KEY) {
				if (has_alpha && v[3] < TRANSPARENT_THRES) {
					continue;
				}
				v[3] = 0;
				idx = RGBA_TO_INDEX(v);
			} else {
				idx = RGBA_TO_INDEX(v);
			}
			dist[idx]++;
			qc->count++;
		}
	}
}

/**
 * 指定領域の最小値、最大値、分散、中央値を求める
 */
static void calculate_quant_color(uint32_t *dist, QuantColor *qc)
{
	int v[4];
	int i, j;
	int med;
	double sum[4] = {0.0, 0.0, 0.0, 0.0};
	double sum2[4] = {0.0, 0.0, 0.0, 0.0};
	uint32_t axis[4][32];
	int i_min[4];
	int i_max[4];

	memcpy(i_min, qc->min, sizeof(i_min));
	memcpy(i_max, qc->max, sizeof(i_max));

	memset(qc->min, 1, sizeof(qc->min));
	memset(qc->max, 0, sizeof(qc->max));
	memset(axis, 0, sizeof(axis));
	qc->count = 0;

	for (v[3] = i_min[3]; v[3] < i_max[3]; v[3]++) {
		for (v[2] = i_min[2]; v[2] < i_max[2]; v[2]++) {
			for (v[1] = i_min[1]; v[1] < i_max[1]; v[1]++) {
				for (v[0] = i_min[0]; v[0] < i_max[0]; v[0]++) {
					uint32_t n = dist[RGBA_TO_INDEX(v)];
					if (n == 0) {
						continue;
					}
					for (i = 0; i < 4; i++) {
						int w = v[i];
						if (qc->min[i] > w) {
							qc->min[i] = w;
						}
						if (qc->max[i] <= w) {
							qc->max[i] = w + 1;
						}
						sum[i] += w * n;
						sum2[i] += w * w * n;
						axis[i][w] += n;
					}
					qc->count += n;
				}
			}
		}
	}
	if (qc->count == 0) {
		for (i = 0; i < 4; i++) {
			int ave = (qc->min[i] + qc->max[i]) / 2;
			int d = qc->max[i] - qc->min[i];

			qc->ave[i] = ave;
			qc->med[i] = ave;
			qc->var[i] = d * d / 16;
		}
		return;
	}
	med = qc->count / 2;

	for (i = 0; i < 4; i++) {
		// 平均(最近傍丸め)
		qc->ave[i] = (sum[i] + med) / qc->count;
		// 分散
		qc->var[i] = (sum2[i] - sum[i] * sum[i] / qc->count) / qc->count;
	}
	for (i = 0; i < 4; i++) {
		int acc = 0;
		// 中央値
		for (j = 0; j < 32; j++) {
			acc += axis[i][j];
			if (acc > med) {
				if (j <= qc->min[i]) {
					j++;
				} else if (j >= qc->max[i]) {
					j--;
				}
				qc->med[i] = j;
				break;
			}
		}
	}
}

/**
 * 色クラスタを分割
 * 分散の最も大きい軸を中央値で分割
 */
static void median_cut(uint32_t *dist, QuantColor *qc, int *max_qc, int mode)
{
	int n_qc = 1;
	int i, j;

	while (n_qc < *max_qc) {
		// 分割する箇所を探す
		double max_var = 1.0;
		QuantColor *q_div = NULL;
		QuantColor *q_idx;
		int axis;

		for (i = 0; i < n_qc; i++) {
			QuantColor *q = &qc[i];
			for (j = 0; j < 4; j++) {
				double var = q->var[j];
				if (mode == QUANT_MODE_RGBA) {
					// アルファ値で重みつけ
					if (j < 3) {
						var *= q->ave[3];
					} else {
						var *= 16;
					}
				}
				if (max_var < var) {
					q_div = q;
					axis = j;
					max_var = var;
				}
			}
		}
		if (q_div == NULL) {
			// 分割する箇所が無い
			break;
		}
		q_idx = &qc[n_qc];
		n_qc++;

		// q_divideを、軸axisで分割
		memcpy(q_idx->min, q_div->min, sizeof(q_idx->min));
		memcpy(q_idx->max, q_div->max, sizeof(q_idx->max));
		q_div->max[axis] = q_div->med[axis];
		q_idx->min[axis] = q_div->med[axis];

		calculate_quant_color(dist, q_div);
		calculate_quant_color(dist, q_idx);
	}
	*max_qc = n_qc;
}

// pal   : RefImageのパレット
// table : 色からindexに変換するテーブル
static void quant_color_to_palette(uint32_t *pal, uint8_t *table, QuantColor *qc, int n_qc, int mode)
{
	int i;

	for (i = 0; i < n_qc; i++) {
		QuantColor *q = &qc[i];
		int r = (q->ave[0] << RGB_SHIFT) | (q->ave[0] >> 2);
		int g = (q->ave[1] << RGB_SHIFT) | (q->ave[1] >> 2);
		int b = (q->ave[2] << RGB_SHIFT) | (q->ave[2] >> 2);
		int a;

		if (mode == QUANT_MODE_RGBA) {
			// 不透明領域がわずかに透けると目立つ
			if (q->ave[3] >= 7) {
				a = 255;
			} else {
				a = (q->ave[3] << 5)   | (q->ave[3] << 1) | (q->ave[3] >> 2);
			}
		} else {
			a = 255;
		}

		pal[i] = (r << COLOR_R_SHIFT) | (g << COLOR_G_SHIFT) | (b << COLOR_B_SHIFT) | (a << COLOR_A_SHIFT);
//		fprintf(stderr, "#%02x%02x%02x/%d\n", r, g, b, a);
	}

	for (i = 0; i < n_qc; i++) {
		int v[4];
		QuantColor *q = &qc[i];

		for (v[3] = q->min[3]; v[3] < q->max[3]; v[3]++) {
			for (v[2] = q->min[2]; v[2] < q->max[2]; v[2]++) {
				for (v[1] = q->min[1]; v[1] < q->max[1]; v[1]++) {
					for (v[0] = q->min[0]; v[0] < q->max[0]; v[0]++) {
						table[RGBA_TO_INDEX(v)] = i;
					}
				}
			}
		}
	}
}
// 最近傍
static void truecolor_to_palette(RefImage *dst, const RefImage *src, int mode, uint8_t *table)
{
	int x, y;
	int width = dst->width;
	int height = dst->height;
	int has_alpha = (src->bands == BAND_RGBA);
	int channels = (has_alpha ? 4 : 3);

	for (y = 0; y < height; y++) {
		const uint8_t *p = src->data + src->pitch * y;
		uint8_t *p_dst = dst->data + dst->pitch * y;
		for (x = 0; x < width; x++) {
			int v[4];

			v[0] = p[0] >> RGB_SHIFT;
			v[1] = p[1] >> RGB_SHIFT;
			v[2] = p[2] >> RGB_SHIFT;
			if (mode == QUANT_MODE_RGB) {
				// RGBモードの場合は、v[3]は常に0
				v[3] = 0;
			} else {
				if (has_alpha) {
					v[3] = p[3] >> A_SHIFT;
				} else {
					v[3] = A_MAX;
				}
			}
			p += channels;

			if (mode == QUANT_MODE_KEY) {
				if (has_alpha && v[3] < TRANSPARENT_THRES) {
					p_dst[x] = 255;
				} else {
					v[3] = 0;
					p_dst[x] = table[RGBA_TO_INDEX(v)];
				}
			} else {
				p_dst[x] = table[RGBA_TO_INDEX(v)];
			}
		}
	}
}
// Floyd-Steinberg dithering
static void truecolor_to_palette_dither(RefImage *dst, const RefImage *src, int mode, uint8_t *table)
{
	int x, y, i;
	int width = dst->width;
	int height = dst->height;
	int pitch = src->pitch;
	int has_alpha = (src->bands == BAND_RGBA);
	int channels = (has_alpha ? 4 : 3);
	uint8_t *cur_line = malloc(pitch);
	uint8_t *next_line = NULL;
	uint8_t *palette = malloc(PALETTE_NUM * 4);

	for (i = 0; i < PALETTE_NUM; i++) {
		uint32_t pal = dst->palette[i];
		uint8_t *p = &palette[i * 4];
		p[0] = (pal & COLOR_R_MASK) >> COLOR_R_SHIFT;
		p[1] = (pal & COLOR_G_MASK) >> COLOR_G_SHIFT;
		p[2] = (pal & COLOR_B_MASK) >> COLOR_B_SHIFT;
		p[3] = (pal & COLOR_A_MASK) >> COLOR_A_SHIFT;
	}

	for (y = 0; y < height; y++) {
		const uint8_t *p_src = src->data + src->pitch * y;
		uint8_t *p_dst = dst->data + dst->pitch * y;
		uint8_t *s1, *s2;

		if (next_line != NULL) {
			uint8_t *tmp = cur_line;
			cur_line = next_line;
			next_line = tmp;
		} else {
			memcpy(cur_line, p_src, pitch);
			next_line = malloc(pitch);
		}
		if (y + 1 < height) {
			memcpy(next_line, p_src + pitch, pitch);
		} else {
			memset(next_line, 0, pitch);
		}
		s1 = cur_line;
		s2 = next_line;

		for (x = 0; x < width; x++) {
			int v[4];
			int d[4];
			int idx;

			v[0] = s1[0] >> RGB_SHIFT;
			v[1] = s1[1] >> RGB_SHIFT;
			v[2] = s1[2] >> RGB_SHIFT;
			if (mode == QUANT_MODE_RGB) {
				// RGBモードの場合は、v[3]は常に0
				v[3] = 0;
			} else {
				if (has_alpha) {
					v[3] = s1[3] >> A_SHIFT;
				} else {
					v[3] = A_MAX;
				}
			}

			if (mode == QUANT_MODE_KEY) {
				if (has_alpha && v[3] < TRANSPARENT_THRES) {
					idx = 255;
				} else {
					v[3] = 0;
					idx = table[RGBA_TO_INDEX(v)];
				}
			} else {
				idx = table[RGBA_TO_INDEX(v)];
			}
			*p_dst = idx;

			// 誤差を計算
			for (i = 0; i < channels; i++) {
				d[i] = s1[i] - palette[idx * 4 + i];
			}
			// 誤差を分配
			if (x + 1 < width) {
				for (i = 0; i < channels; i++) {
					int j = s1[i + channels] + d[i] * 7 / 16;
					if (j < 0) {
						j = 0;
					} else if (j > 255) {
						j = 255;
					}
					s1[i + channels] = j;
				}
			}
			if (y + 1 < height) {
				if (x > 0) {
					for (i = 0; i < channels; i++) {
						int j = s2[i - channels] + d[i] * 3 / 16;
						if (j < 0) {
							j = 0;
						} else if (j > 255) {
							j = 255;
						}
						s2[i - channels] = j;
					}
				}
				for (i = 0; i < channels; i++) {
					int j = s2[i] + d[i] * 5 / 16;
					if (j < 0) {
						j = 0;
					} else if (j > 255) {
						j = 255;
					}
					s2[i] = j;
				}
				if (x + 1 < width) {
					for (i = 0; i < channels; i++) {
						int j = s1[i + channels] + d[i] / 16;
						if (j < 0) {
							j = 0;
						} else if (j > 255) {
							j = 255;
						}
						s1[i + channels] = j;
					}
				}
			}

			p_src += channels;
			s1 += channels;
			s2 += channels;
			p_dst++;
		}
	}
	free(cur_line);
	free(next_line);
	free(palette);
}

/**
 * 256色に減色
 */
void quantize(RefImage *dst, const RefImage *src, int mode, int dither)
{
    int n_distri_size = (mode == QUANT_MODE_RGBA ? HIST_SIZE_RGBA : HIST_SIZE_RGB);
    int n_qc = (mode == QUANT_MODE_KEY ? 255 : 256);

	uint32_t *dist;
    QuantColor *qc = malloc(sizeof(QuantColor) * n_qc);
    memset(qc, 0, sizeof(QuantColor) * n_qc);
	
	dist = malloc(sizeof(uint32_t) * n_distri_size);
	memset(dist, 0, sizeof(uint32_t) * n_distri_size);

    sample_color_distribution(dist, qc, src, mode);
	if (qc[0].count > 0) {
		memset(qc[0].min, 0, sizeof(qc[0].min));
		qc[0].max[0] = RGB_MAX + 1;
		qc[0].max[1] = RGB_MAX + 1;
		qc[0].max[2] = RGB_MAX + 1;

		if (mode == QUANT_MODE_RGBA) {
			qc[0].max[3] = A_MAX + 1;
		} else {
			qc[0].max[3] = 1;
		}
		calculate_quant_color(dist, qc);
		median_cut(dist, qc, &n_qc, mode);
		quant_color_to_palette(dst->palette, (uint8_t*)dist, qc, n_qc, mode);
		if (dither) {
			//truecolor_to_palette_dither(dst, src, mode, (uint8_t*)dist);
			uint8_t *npal = get_nearest_palette(dst->palette, mode);
			truecolor_to_palette_dither(dst, src, mode, npal);
			free(npal);
		} else {
			truecolor_to_palette(dst, src, mode, (uint8_t*)dist);
		}
	} else {
		// QUANT_MODE_KEYで、すべて透明ピクセル
		memset(dst->data, 255, dst->pitch * dst->height);
	}
}

///////////////////////////////////////////////////////////////////////////////

enum {
	QPAL_SIZE = 32,
	QPAL_SIZE_A = 8,
	QPAL_SIZE_RGB = QPAL_SIZE * QPAL_SIZE * QPAL_SIZE,
	QPAL_SIZE_RGBA = QPAL_SIZE * QPAL_SIZE * QPAL_SIZE * QPAL_SIZE_A,
};

int get_palette_mode(uint32_t *palette)
{
	int mode = QUANT_MODE_RGB;
	int i;

	for (i = 0; i < PALETTE_NUM; i++) {
		uint32_t c = palette[i];
		if ((c & COLOR_A_MASK) != COLOR_A_MASK) {
			mode = QUANT_MODE_RGBA;
			break;
		}
	}
	return mode;
}
uint8_t *get_nearest_palette(uint32_t *palette, int mode)
{
	int i;
	double r, g, b, a;
	double (*pal)[4] = malloc(sizeof(double) * PALETTE_NUM * 4);

    int n_dist_size = (mode == QUANT_MODE_RGBA ? QPAL_SIZE_RGBA : QPAL_SIZE_RGB);
	uint8_t *p_dist = malloc(sizeof(uint8_t) * n_dist_size);
	uint8_t *dist = p_dist;
	memset(p_dist, 0, sizeof(uint8_t) * n_dist_size);

	for (i = 0; i < PALETTE_NUM; i++) {
		uint32_t c = palette[i];
		pal[i][0] = (double)(((c & COLOR_R_MASK) >> COLOR_R_SHIFT) >> RGB_SHIFT);
		pal[i][1] = (double)(((c & COLOR_G_MASK) >> COLOR_G_SHIFT) >> RGB_SHIFT);
		pal[i][2] = (double)(((c & COLOR_B_MASK) >> COLOR_B_SHIFT) >> RGB_SHIFT);
		pal[i][3] = (double)(((c & COLOR_A_MASK) >> COLOR_A_SHIFT) >> A_SHIFT);
	}

	if (mode == QUANT_MODE_RGBA) {
		a = 0;
	} else {
		a = QPAL_SIZE_A - 1;
	}
	for (; a < QPAL_SIZE_A; a += 1.0) {
		for (b = 0; b < QPAL_SIZE; b += 1.0) {
			for (g = 0; g < QPAL_SIZE; g += 1.0) {
				for (r = 0; r < QPAL_SIZE; r += 1.0) {
					int idx = 0;
					double min = 4294967296.0;
					// 最小距離を求める
					for (i = 0; i < PALETTE_NUM; i++) {
						double dr = r - pal[i][0];
						double dg = g - pal[i][1];
						double db = b - pal[i][2];
						double da = (a - pal[i][3]) * 4.0;
						double d = dr*dr + dg*dg + db*db + da*da;
						if (min > d) {
							min = d;
							idx = i;
							if (d == 0.0) {
								break;
							}
						}
					}
					*dist++ = idx;
				}
			}
		}
	}

	free(pal);
	return p_dist;
}

// srcからdstのもっとも近い色に変換するテーブル
uint8_t *get_convert_palette_table(uint32_t *dst, const uint32_t *src)
{
	uint8_t *p = malloc(PALETTE_NUM);
	double (*sd)[4] = malloc(sizeof(double) * 4 * PALETTE_NUM);
	double (*dd)[4] = malloc(sizeof(double) * 4 * PALETTE_NUM);
	int i;

	// 浮動小数点数に変換
	for (i = 0; i < PALETTE_NUM; i++) {
		uint32_t c = src[i];
		sd[i][0] = (double)(((c & COLOR_R_MASK) >> COLOR_R_SHIFT) >> RGB_SHIFT);
		sd[i][1] = (double)(((c & COLOR_G_MASK) >> COLOR_G_SHIFT) >> RGB_SHIFT);
		sd[i][2] = (double)(((c & COLOR_B_MASK) >> COLOR_B_SHIFT) >> RGB_SHIFT);
		sd[i][3] = (double)(((c & COLOR_A_MASK) >> COLOR_A_SHIFT) >> A_SHIFT);
	}
	for (i = 0; i < PALETTE_NUM; i++) {
		uint32_t c = dst[i];
		dd[i][0] = (double)(((c & COLOR_R_MASK) >> COLOR_R_SHIFT) >> RGB_SHIFT);
		dd[i][1] = (double)(((c & COLOR_G_MASK) >> COLOR_G_SHIFT) >> RGB_SHIFT);
		dd[i][2] = (double)(((c & COLOR_B_MASK) >> COLOR_B_SHIFT) >> RGB_SHIFT);
		dd[i][3] = (double)(((c & COLOR_A_MASK) >> COLOR_A_SHIFT) >> A_SHIFT);
	}

	// もっとも近い色を探す
	for (i = 0; i < PALETTE_NUM; i++) {
		int j;
		int idx = 0;
		double min = 4294967296.0;
		double r = sd[i][0];
		double g = sd[i][1];
		double b = sd[i][2];
		double a = sd[i][3];

		for (j = 0; j < PALETTE_NUM; j++) {
			double dr = r - dd[j][0];
			double dg = g - dd[j][1];
			double db = b - dd[j][2];
			double da = (a - dd[j][3]) * 4.0;
			double d = dr*dr + dg*dg + db*db + da*da;
			if (min > d) {
				min = d;
				idx = j;
				if (d == 0.0) {
					break;
				}
			}
		}
		p[i] = idx;
	}
	free(sd);
	free(dd);

	return p;
}

/**
 * 指定したパレットに減色
 */
void quantize_palette(RefImage *dst, const RefImage *src, int dither)
{
	int mode = get_palette_mode(dst->palette);
	uint8_t *dist = get_nearest_palette(dst->palette, mode);
	int src_mode = (src->bands == BAND_RGBA ? QUANT_MODE_RGBA : QUANT_MODE_RGB);

	if (dither) {
		truecolor_to_palette_dither(dst, src, src_mode, dist);
	} else {
		truecolor_to_palette(dst, src, src_mode, dist);
	}

	free(dist);
}
