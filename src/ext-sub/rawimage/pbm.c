#include "image_raw.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


enum {
	STREAM_GET_EOF = -1,
	STREAM_GET_ERROR = -2,
};
enum {
	PARSE_SPACE,
	PARSE_COMMENT,
	PARSE_DIGIT,
};

typedef struct {
	Value r;
	char buf[BUFFER_SIZE];
	int cur;
	int max;
} ReadBuffer;

typedef struct {
	int type;
	int width, height;
	int max;
} PNMHeader;


static ReadBuffer *ReadBuffer_new(Value r)
{
	ReadBuffer *rb = malloc(sizeof(ReadBuffer));
	rb->r = r;
	rb->cur = 0;
	rb->max = 0;
	return rb;
}
static void ReadBuffer_format_error(ReadBuffer *rb)
{
	fs->throw_errorf(mod_image, "ImageError", "Invalid PNM format");
}
static void ReadBuffer_close(ReadBuffer *rb)
{
	free(rb);
}

static int ReadBuffer_readbin(ReadBuffer *rb, char *p, int *psize)
{
	int read_to = *psize;   // 読み込むサイズ
	int last = read_to;     // 残りサイズ
	int nread = 0;          // 読み込み済み

	for (;;) {
		// バッファの最後か読み込みサイズまで読む
		if (rb->max > 0) {
			int sz = rb->max - rb->cur;
			if (sz > last) {
				sz = last;
			}
			memcpy(p + nread, rb->buf + rb->cur, sz);
			nread += sz;
			last -= sz;
			rb->cur += sz;
		}
		// 全て読んだら終了
		if (last <= 0) {
			break;
		}
		rb->cur = 0;
		rb->max = BUFFER_SIZE;
		if (!fs->stream_read_data(rb->r, NULL, rb->buf, &rb->max, FALSE, FALSE)) {
			return FALSE;
		}
		// 読み取れなかったら終了
		if (rb->max == 0) {
			break;
		}
	}
	return TRUE;
}
/**
 * 整数を読み込む
 * 空白、改行は無視
 * #から改行までは無視
 * それ以外はエラー
 */
static int ReadBuffer_readint(ReadBuffer *rb, int *pvalue)
{
	int value = 0;
	int phase = PARSE_SPACE;

	for (;;) {
		if (rb->max > 0) {
			int i;
			for (i = rb->cur; i < rb->max; i++) {
				int ch = rb->buf[i] & 0xFF;
				switch (phase) {
				case PARSE_SPACE:
					if (isdigit(ch)) {
						phase = PARSE_DIGIT;
						value = ch - '0';
					} else if (ch == '#') {
						phase = PARSE_COMMENT;
					} else if (!isspace(ch)) {
						ReadBuffer_format_error(rb);
						return FALSE;
					}
					break;
				case PARSE_COMMENT:
					if (ch == '\n') {
						phase = PARSE_SPACE;
					}
					break;
				case PARSE_DIGIT:
					if (isdigit(ch)) {
						value = value * 10 + (ch - '0');
					} else {
						*pvalue = value;
						rb->cur = i;
						return TRUE;
					}
					break;
				}
			}
		}
		rb->cur = 0;
		rb->max = BUFFER_SIZE;
		if (!fs->stream_read_data(rb->r, NULL, rb->buf, &rb->max, FALSE, FALSE)) {
			return FALSE;
		}
		// 読み取れなかったらエラー
		if (rb->max == 0) {
			ReadBuffer_format_error(rb);
			return FALSE;
		}
	}
	return TRUE;
}
/**
 * 0または1の連続を読み込む
 * 空白は無視
 */
static int ReadBuffer_read01(ReadBuffer *rb, char *p, int *psize)
{
	int read_to = *psize;   // 読み込むサイズ
	int last = read_to;     // 残りサイズ
	int nread = 0;          // 読み込み済み

	for (;;) {
		// バッファの最後か読み込みサイズまで読む
		if (rb->max > 0) {
			int i;
			for (i = rb->cur; i < rb->max; i++) {
				int ch = rb->buf[i] & 0xFF;
				if (ch == '0' || ch == '1') {
					p[nread++] = (ch == '1' ? 0 : 255);  // 0:white, 1:black
					last--;
					if (last == 0) {
						*psize = nread;
						rb->cur = i + 1;
						return TRUE;
					}
				}
			}
			rb->cur = i;
		}
		// 全て読んだら終了
		if (last <= 0) {
			break;
		}
		rb->cur = 0;
		rb->max = BUFFER_SIZE;
		if (!fs->stream_read_data(rb->r, NULL, rb->buf, &rb->max, FALSE, FALSE)) {
			return FALSE;
		}
		// 読み取れなかったら終了
		if (rb->max == 0) {
			break;
		}
	}
	return TRUE;
}
/**
 * 次の改行(\n)の次にカーソルを進める
 */
static int ReadBuffer_nextline(ReadBuffer *rb)
{
	for (;;) {
		if (rb->max > 0) {
			int i;
			for (i = rb->cur; i < rb->max; i++) {
				if (rb->buf[i] == '\n') {
					rb->cur = i + 1;
					return TRUE;
				}
			}
		}
		rb->cur = 0;
		rb->max = BUFFER_SIZE;
		if (!fs->stream_read_data(rb->r, NULL, rb->buf, &rb->max, FALSE, FALSE)) {
			return FALSE;
		}
		// 読み取れなかったら終了
		if (rb->max == 0) {
			break;
		}
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static int detect_image_type(PNMHeader *hdr, ReadBuffer *rb)
{
	char magic[2];
	int sz = 2;

	if (!ReadBuffer_readbin(rb, magic, &sz)) {
		return FALSE;
	}
	if (sz < 2 || magic[0] != 'P') {
		ReadBuffer_format_error(rb);
		return FALSE;
	}
	if (magic[1] >= '1' && magic[1] <= '6') {
		hdr->type = magic[1] - '0';
	} else {
		ReadBuffer_format_error(rb);
		return FALSE;
	}
	if (!ReadBuffer_readint(rb, &hdr->width)) {
		return FALSE;
	}
	if (!ReadBuffer_readint(rb, &hdr->height)) {
		return FALSE;
	}
	if (hdr->type == TYPE_PBM_ASCII || hdr->type == TYPE_PBM_BIN) {
		// 最大値+1
		hdr->max = 2;
	} else {
		if (!ReadBuffer_readint(rb, &hdr->max)) {
			return FALSE;
		}
		// 最大値+1
		hdr->max += 1;
	}
	if (!ReadBuffer_nextline(rb)) {
		return FALSE;
	}

	return TRUE;
}

static int load_pbm_ascii(RefImage *img, PNMHeader *hdr, ReadBuffer *rb)
{
	int y;
	for (y = 0; y < img->height; y++) {
		uint8_t *p = img->data + y * img->pitch;
		int rd = img->pitch;

		if (!ReadBuffer_read01(rb, (char*)p, &rd)) {
			return FALSE;
		}
		if (rd < img->pitch) {
			ReadBuffer_format_error(rb);
			return FALSE;
		}
	}
	return TRUE;
}
static int load_pgm_ppm_ascii(RefImage *img, PNMHeader *hdr, ReadBuffer *rb)
{
	int y;
	int max = hdr->max;

	for (y = 0; y < img->height; y++) {
		uint8_t *p = img->data + y * img->pitch;
		int x;

		for (x = 0; x < img->pitch; x++) {
			int val;
			if (!ReadBuffer_readint(rb, &val)) {
				return FALSE;
			}
			if (max < 256) {
				val = val * 256 / max;
				if (val > 255) {
					val = 255;
				}
			}
			p[x] = val;
		}
	}
	return TRUE;
}

static int load_pbm_bin(RefImage *img, PNMHeader *hdr, ReadBuffer *rb)
{
	int y;
	int rd_pitch = (img->pitch + 7) / 8;
	uint8_t *buf = malloc(rd_pitch);

	for (y = 0; y < img->height; y++) {
		uint8_t *p = img->data + y * img->pitch;
		int rd = rd_pitch;
		int x;

		if (!ReadBuffer_readbin(rb, (char*)buf, &rd)) {
			free(buf);
			return FALSE;
		}
		if (rd < rd_pitch) {
			ReadBuffer_format_error(rb);
			free(buf);
			return FALSE;
		}
		for (x = 0; x < img->width; x++) {
			if ((buf[x / 8] & (1 << (7 - (x % 8)))) != 0) {
				p[x] = 0;
			} else {
				p[x] = 255;
			}
		}
	}
	free(buf);
	return TRUE;
}
static int load_pgm_ppm_bin(RefImage *img, PNMHeader *hdr, ReadBuffer *rb)
{
	int y;
	for (y = 0; y < img->height; y++) {
		uint8_t *p = img->data + y * img->pitch;
		int rd = img->pitch;

		if (!ReadBuffer_readbin(rb, (char*)p, &rd)) {
			return FALSE;
		}
		if (rd < img->pitch) {
			ReadBuffer_format_error(rb);
			return FALSE;
		}
		if (hdr->max < 256) {
			int max = hdr->max;
			int x;
			for (x = 0; x < img->pitch; x++) {
				int val = p[x] * 256 / max;
				if (val > 255) {
					val = 255;
				}
				p[x] = val;
			}
		}
	}
	return TRUE;
}

int load_image_pnm_sub(RefImage *image, Value r, int info_only)
{
	PNMHeader hdr;
	ReadBuffer *rb = ReadBuffer_new(r);

	if (!detect_image_type(&hdr, rb)) {
		goto ERROR_END;
	}

	if (hdr.width == 0 || hdr.height == 0) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "width and height must be more than 0");
		goto ERROR_END;
	}
	if (hdr.width > MAX_IMAGE_SIZE || hdr.height > MAX_IMAGE_SIZE) {
		fs->throw_errorf(mod_image, "ImageError", "Image size too large (max:%d)", MAX_IMAGE_SIZE);
		goto ERROR_END;
	}

	image->width = hdr.width;
	image->height = hdr.height;

	switch (hdr.type) {
	case TYPE_PBM_ASCII:
	case TYPE_PGM_ASCII:
	case TYPE_PBM_BIN:
	case TYPE_PGM_BIN:
		image->bands = BAND_L;
		image->pitch = hdr.width;
		break;
	case TYPE_PPM_ASCII:
	case TYPE_PPM_BIN:
		image->bands = BAND_RGB;
		image->pitch = hdr.width * 3;
		break;
	}
	if (!info_only) {
		image->data = malloc(image->pitch * image->height);

		switch (hdr.type) {
		case TYPE_PBM_ASCII:
			if (!load_pbm_ascii(image, &hdr, rb)) {
				goto ERROR_END;
			}
			break;
		case TYPE_PGM_ASCII:
		case TYPE_PPM_ASCII:
			if (!load_pgm_ppm_ascii(image, &hdr, rb)) {
				goto ERROR_END;
			}
			break;
		case TYPE_PBM_BIN:
			if (!load_pbm_bin(image, &hdr, rb)) {
				goto ERROR_END;
			}
			break;
		case TYPE_PGM_BIN:
		case TYPE_PPM_BIN:
			if (!load_pgm_ppm_bin(image, &hdr, rb)) {
				goto ERROR_END;
			}
			break;
		}
	}

	ReadBuffer_close(rb);
	return TRUE;

ERROR_END:
	ReadBuffer_close(rb);
	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int save_pnm_header(Value w, RefImage *image, int type)
{
	char buf[32];

	buf[0] = 'P';
	buf[1] = (type + '0');
	buf[2] = '\n';

	if (!fs->stream_write_data(w, buf, 3)) {
		return FALSE;
	}
	sprintf(buf, "%d %d\n", image->width, image->height);
	if (!fs->stream_write_data(w, buf, -1)) {
		return FALSE;
	}
	if (type != TYPE_PBM_ASCII && type != TYPE_PBM_BIN) {
		if (!fs->stream_write_data(w, "255\n", 4)) {
			return FALSE;
		}
	}

	return TRUE;
}
static int save_pbm(Value w, RefImage *img, int ascii, int threshold)
{
	int x, y;
	int pitch = (img->width + 7) / 8;
	uint8_t *dst = malloc(pitch);
	uint8_t *palette = NULL;
	char *dst_c = NULL;

	if (ascii) {
		dst_c = malloc(img->width + 2);
	}
	if (img->bands == BAND_P) {
		palette = malloc(PALETTE_NUM);
		for (x = 0; x < PALETTE_NUM; x++) {
			uint32_t col = img->palette[x];
			int r = (col & COLOR_R_MASK) >> COLOR_R_SHIFT;
			int g = (col & COLOR_G_MASK) >> COLOR_G_SHIFT;
			int b = (col & COLOR_B_MASK) >> COLOR_B_SHIFT;
			palette[x] = (r * 77 + g * 150 + b * 29) < threshold * 256 ? 1 : 0;
		}
	}

	for (y = 0; y < img->height; y++) {
		const uint8_t *src = img->data + y * img->pitch;
		memset(dst, 0, pitch);

		switch (img->bands) {
		case BAND_P:
			for (x = 0; x < img->width; x++) {
				if (palette[src[x]]) {
					dst[x / 8] |= 1 << (7 - (x % 8));
				}
			}
			break;
		case BAND_L:
			for (x = 0; x < img->width; x++) {
				if (src[x] < threshold) {
					dst[x / 8] |= 1 << (7 - (x % 8));
				}
			}
			break;
		case BAND_LA:
			for (x = 0; x < img->width; x++) {
				if (src[x * 2] < threshold) {
					dst[x / 8] |= 1 << (7 - (x % 8));
				}
			}
			break;
		case BAND_RGB:
			for (x = 0; x < img->width; x++) {
				int r = src[x * 3 + 0];
				int g = src[x * 3 + 1];
				int b = src[x * 3 + 2];
				if ((r * 77 + g * 150 + b * 29) < threshold * 256) {
					dst[x / 8] |= 1 << (7 - (x % 8));
				}
			}
			break;
		case BAND_RGBA:
			for (x = 0; x < img->width; x++) {
				int r = src[x * 4 + 0];
				int g = src[x * 4 + 1];
				int b = src[x * 4 + 2];
				if ((r * 77 + g * 150 + b * 29) < threshold * 256) {
					dst[x / 8] |= 1 << (7 - (x % 8));
				}
			}
			break;
		}
		if (ascii) {
			int n = img->width;
			for (x = 0; x < n; x++) {
				dst_c[x] = ((dst[x / 8] & 1 << (7 - (x % 8))) != 0) ? '1' : '0';
			}
			dst_c[x] = '\n';
			if (!fs->stream_write_data(w, dst_c, x + 1)) {
				goto ERROR_END;
			}
		} else {
			if (!fs->stream_write_data(w, (const char*)dst, pitch)) {
				goto ERROR_END;
			}
		}
	}
	free(dst);
	free(palette);
	free(dst_c);
	return TRUE;

ERROR_END:
	free(dst);
	free(palette);
	free(dst_c);
	return FALSE;
}
static int save_pgm(Value w, RefImage *img, int ascii)
{
	int x, y;
	uint8_t *dst = malloc(img->width * 3);
	uint8_t *palette = NULL;

	if (img->bands == BAND_P) {
		palette = malloc(PALETTE_NUM);
		for (x = 0; x < PALETTE_NUM; x++) {
			uint32_t col = img->palette[x];
			int r = (col & COLOR_R_MASK) >> COLOR_R_SHIFT;
			int g = (col & COLOR_G_MASK) >> COLOR_G_SHIFT;
			int b = (col & COLOR_B_MASK) >> COLOR_B_SHIFT;
			palette[x] = (r * 77 + g * 150 + b * 29) / 256;
		}
	}

	for (y = 0; y < img->height; y++) {
		const uint8_t *src = img->data + y * img->pitch;

		switch (img->bands) {
		case BAND_P:
			for (x = 0; x < img->width; x++) {
				dst[x] = palette[src[x]];
			}
			break;
		case BAND_L:
			memcpy(dst, src, img->width);
			break;
		case BAND_LA:
			for (x = 0; x < img->width; x++) {
				dst[x] = src[x * 2];
			}
			break;
		case BAND_RGB:
			for (x = 0; x < img->width; x++) {
				int r = src[x * 3 + 0];
				int g = src[x * 3 + 1];
				int b = src[x * 3 + 2];
				dst[x] = (r * 77 + g * 150 + b * 29) / 256;
			}
			break;
		case BAND_RGBA:
			for (x = 0; x < img->width; x++) {
				int r = src[x * 4 + 0];
				int g = src[x * 4 + 1];
				int b = src[x * 4 + 2];
				dst[x] = (r * 77 + g * 150 + b * 29) / 256;
			}
			break;
		}
		if (ascii) {
			int n = img->width;
			for (x = 0; x < n; x++) {
				char tmp[8];
				sprintf(tmp, "%d%c", dst[x], (x < n - 1) ? ' ' : '\n');
				if (!fs->stream_write_data(w, tmp, -1)) {
					free(dst);
					free(palette);
					return FALSE;
				}
			}
		} else {
			if (!fs->stream_write_data(w, (const char*)dst, img->width)) {
				free(dst);
				free(palette);
				return FALSE;
			}
		}
	}
	free(dst);
	free(palette);
	return TRUE;
}
static int save_ppm(Value w, RefImage *img, int ascii)
{
	int x, y;
	uint8_t *dst = malloc(img->width);

	for (y = 0; y < img->height; y++) {
		const uint8_t *src = img->data + y * img->pitch;
		uint8_t *p = dst;

		switch (img->bands) {
		case BAND_P:
			for (x = 0; x < img->width; x++) {
				uint32_t col = img->palette[src[x]];
				*p++ = (col & COLOR_R_MASK) >> COLOR_R_SHIFT;
				*p++ = (col & COLOR_G_MASK) >> COLOR_G_SHIFT;
				*p++ = (col & COLOR_B_MASK) >> COLOR_B_SHIFT;
			}
			break;
		case BAND_L:
			for (x = 0; x < img->width; x++) {
				int v = src[x];
				*p++ = v;
				*p++ = v;
				*p++ = v;
			}
			break;
		case BAND_LA:
			for (x = 0; x < img->width; x++) {
				int v = src[x * 2];
				*p++ = v;
				*p++ = v;
				*p++ = v;
			}
			break;
		case BAND_RGB:
			memcpy(dst, src, img->width * 3);
			break;
		case BAND_RGBA:
			for (x = 0; x < img->width; x++) {
				*p++ = src[x * 4 + 0];
				*p++ = src[x * 4 + 1];
				*p++ = src[x * 4 + 2];
			}
			break;
		}
		if (ascii) {
			int n = img->width * 3;
			for (x = 0; x < n; x++) {
				char tmp[8];
				sprintf(tmp, "%d%c", dst[x], (x < n - 1) ? ' ' : '\n');
				if (!fs->stream_write_data(w, tmp, -1)) {
					free(dst);
					return FALSE;
				}
			}
		} else {
			if (!fs->stream_write_data(w, (const char*)dst, img->width * 3)) {
				free(dst);
				return FALSE;
			}
		}
	}
	free(dst);
	return TRUE;
}

int save_image_pnm_sub(RefImage *image, Value w, int type, int threshold)
{
	if (!save_pnm_header(w, image, type)) {
		return FALSE;
	}
	switch (type) {
	case TYPE_PBM_ASCII:
		if (!save_pbm(w, image, TRUE, threshold)) {
			return FALSE;
		}
		break;
	case TYPE_PGM_ASCII:
		if (!save_pgm(w, image, TRUE)) {
			return FALSE;
		}
		break;
	case TYPE_PPM_ASCII:
		if (!save_ppm(w, image, TRUE)) {
			return FALSE;
		}
		break;
	case TYPE_PBM_BIN:
		if (!save_pbm(w, image, FALSE, threshold)) {
			return FALSE;
		}
		break;
	case TYPE_PGM_BIN:
		if (!save_pgm(w, image, FALSE)) {
			return FALSE;
		}
		break;
	case TYPE_PPM_BIN:
		if (!save_ppm(w, image, FALSE)) {
			return FALSE;
		}
		break;
	}

	return TRUE;
}
