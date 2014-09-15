/*
 * zipfile.c
 *
 *  Created on: 2012/03/18
 *      Author: frog
 */

#include "zipfile.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define CENTRAL_MAGIC "PK\1\2"
#define LOCAL_MAGIC   "PK\3\4"
#define EOCD_MAGIC    "PK\5\6"
#define DATADES_MAGIC "PK\7\8"

enum {
	CENTRAL_SIZE = 46,
	LOCAL_SIZE = 30,
	EOCD_SIZE = 22,
	MAX_ZIP_BUF = 64*1024,
};


void CentralDirEnd_add(CentralDirEnd *cdir, CentralDir *cd, int32_t offset)
{
	CentralDir *p;
	if (cdir->cdir_size >= cdir->cdir_max) {
		if (cdir->cdir_max == 0) {
			cdir->cdir_max = 16;
		} else {
			cdir->cdir_max *= 2;
		}
		cdir->cdir = realloc(cdir->cdir, cdir->cdir_max * sizeof(CentralDir));
		memset(cdir->cdir + cdir->cdir_size, 0, (cdir->cdir_max - cdir->cdir_size) * sizeof(CentralDir));
	}
	p = &cdir->cdir[cdir->cdir_size];
	cdir->cdir_size++;

	*p = *cd;
	fs->StrBuf_init(&p->filename, cd->filename.size);
	fs->StrBuf_add(&p->filename, cd->filename.p, cd->filename.size);
	p->offset = offset;
	memset(&p->data, 0, sizeof(StrBuf));
}
void CentralDirEnd_free(CentralDirEnd *cdir)
{
	if (cdir->cdir != NULL) {
		int i;
		for (i = 0; i < cdir->cdir_size; i++) {
			CentralDir *cd = &cdir->cdir[i];
			StrBuf_close(&cd->filename);
		}
		free(cdir->cdir);
	}
	free(cdir);
}

static int align_pow2(int sz, int min)
{
	if (sz <= 0) {
		return 0;
	}
	while (min < sz) {
		min *= 2;
	}
	return min;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int read_int16(const char *p)
{
	return (p[0] & 0xFF) | (p[1] & 0xFF) << 8;
}
static int read_int32(const char *p)
{
	return (p[0] & 0xFF) | (p[1] & 0xFF) << 8 | (p[2] & 0xFF) << 16 | (p[3] & 0xFF) << 24;
}
static void write_int16(char *p, int val)
{
	p[0] = val & 0xFF;
	p[1] = (val >> 8) & 0xFF;
}
static void write_int32(char *p, int val)
{
	p[0] = val & 0xFF;
	p[1] = (val >> 8) & 0xFF;
	p[2] = (val >> 16) & 0xFF;
	p[3] = (val >> 24) & 0xFF;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MAKE_2C(c1, c2) ((c1) | ((c2) << 8))

static CentralDir *read_local_dir(Value reader)
{
	return NULL;
}

/**
 * 前からLOCAL_MAGICを検索する
 */
CentralDir *find_central_dir(Value reader)
{
	if (fs->Value_type(reader) == fs->cls_bytesio) {
		RefBytesIO *bio = Value_vp(reader);
		int i = bio->cur;
		while (i < bio->buf.size - 4) {
			if (memcmp(&bio->buf.p[i], LOCAL_MAGIC, 4) == 0) {
				bio->cur = i + 4;
				return read_local_dir(reader);
			}
			i++;
		}
	} else {
	}

	return NULL;
}

/**
 * 末尾から22バイトより、遡って探す
 * 最初に見つかったヘッダのオフセットを返す
 */
static CentralDirEnd *find_central_dir_end(char *buf, int size, int offset)
{
	int i;

	if (size < EOCD_SIZE) {
		return NULL;
	}
	for (i = size - EOCD_SIZE; i >= 0; i--) {
		if (memcmp(buf + i, EOCD_MAGIC, 4) == 0) {
			// コメントの長さと末尾の長さが(ほぼ)同じ
			int com_end = i + EOCD_SIZE + read_int16(buf + i + 20);

			if (com_end <= size && com_end + 4 > size) {
				// 分割書庫には対応しないため、0になる
				if (read_int32(buf + i + 4) == 0) {
					// オフセットがだいたい合っているか調べる
					int cdir_size = read_int32(buf + i + 12);
					int cdir_offset = read_int32(buf + i + 16);
					if (cdir_size < MAX_ALLOC && cdir_offset + cdir_size <= offset + i && cdir_offset + cdir_size + 4 > offset + i) {
						CentralDirEnd *cd = malloc(sizeof(CentralDirEnd));
						cd->offset_of_cdir = cdir_offset;
						cd->size_of_cdir = cdir_size;
						cd->cdir_size = read_int16(buf + i + 10);
						cd->cdir_max = align_pow2(cd->cdir_size, 16);
						cd->cdir = NULL;
						return cd;
					}
				}
			}
		}
	}
	return NULL;
}
static void parse_ntfs_field(CentralDir *cdir, Str data, int *time_ntfs)
{
	const char *p = data.p;
	const char *end = p + data.size;

	while (p + 4 < end) {
		int len = read_int16(p + 2);

		switch (p[0] | (p[1] << 8)) {
		case 0x0001:
			if (len >= 8) {
				// FILETIME
				uint32_t lo = read_int32(p + 4);
				uint32_t hi = read_int32(p + 8);
				int64_t i64 = (uint64_t)lo | ((uint64_t)hi << 32);
				cdir->modified = i64 / 10000 - 11644473600000LL;
				*time_ntfs = TRUE;
			}
			break;
		}
		p += len + 4;
	}
}
static void parse_ext_field(CentralDir *cdir, Str data)
{
	const char *p = data.p;
	const char *end = p + data.size;
	int time_ntfs = FALSE;  // NTFS Time優先

	while (p + 4 < end) {
		int len = read_int16(p + 2);

		switch (p[0] | (p[1] << 8)) {
		case MAKE_2C('U', 'T'):
			if (len >= 5 && !time_ntfs) {
				// 更新時刻(Unix epoch UTC)
				cdir->modified = (int64_t)read_int32(p + 5) * 1000LL;
				cdir->time_valid = TRUE;
			}
			break;
		case 0x000a: // NTFS TimeStamp
			if (len >= 4) {
				parse_ntfs_field(cdir, Str_new(p + 8, len - 4), &time_ntfs);
				cdir->time_valid = TRUE;
			}
			break;
		}
		p += len + 4;
	}
}
static int64_t dostime_to_time(int dt, int tm, RefTimeZone *tz)
{
	RefTime d;

	d.cal.year = ((dt >> 9) & 0xFFFF) + 1980;
	d.cal.month = (dt >> 5) & 0xF;
	d.cal.day_of_month = dt & 0x1F;
	d.cal.hour = (tm >> 11) & 0x1F;
	d.cal.minute = (tm >> 5) & 0x3F;
	d.cal.second = (tm & 0x1F) * 2;
	d.cal.millisec = 0;
	d.tz = tz;

	fs->adjust_timezone(&d);
	return d.tm;
}
static void time_to_dostime(int *dt, int *tm, int64_t tm64, RefTimeZone *tz)
{
	RefTime d;

	d.tm = tm64;
	d.tz = tz;
	fs->adjust_date(&d);

	if (d.cal.year < 1980 || d.cal.year > 9999) {
		*dt = (1 << 5) | 1;  // 1980-01-01
		*tm = 0;             // 00:00:00
		return;
	}
	*dt = ((d.cal.year - 1980) << 9) | (d.cal.month << 5) | d.cal.day_of_month;
	*tm = (d.cal.hour << 11) | (d.cal.minute << 5) | (d.cal.second >> 1);
}
static CentralDir *read_central_dirs(CentralDirEnd *cdir_end, char *cbuf, RefCharset *cs, RefTimeZone *tz)
{
	int dir_num = cdir_end->cdir_size;
	CentralDir *cd = malloc(sizeof(CentralDir) * cdir_end->cdir_max);
	int i;
	int offset = 0;

	memset(cd, 0, sizeof(CentralDir) * cdir_end->cdir_max);
	for (i = 0; i < dir_num; i++) {
		CentralDir *p = &cd[i];
		int filename_len, ext_len, comment_len;

		if (offset + CENTRAL_SIZE > cdir_end->size_of_cdir) {
			goto ERROR_END;
		}
		if (memcmp(cbuf + offset, CENTRAL_MAGIC, 4) != 0) {
			goto ERROR_END;
		}
		p->flags = read_int16(cbuf + offset + 8);
		p->method = read_int16(cbuf + offset + 10);


		p->crc32 = read_int32(cbuf + offset + 16);
		p->size_compressed = read_int32(cbuf + offset + 20);
		p->size = read_int32(cbuf + offset + 24);

		filename_len = read_int16(cbuf + offset + 28);
		ext_len = read_int16(cbuf + offset + 30);
		comment_len = read_int16(cbuf + offset + 32);
		if (offset + CENTRAL_SIZE + filename_len + ext_len + comment_len > cdir_end->size_of_cdir) {
			goto ERROR_END;
		}

		fs->StrBuf_init(&p->filename, filename_len);
		fs->StrBuf_add(&p->filename, cbuf + offset + CENTRAL_SIZE, filename_len);

		parse_ext_field(p, Str_new(cbuf + offset + CENTRAL_SIZE + filename_len, ext_len));
		if (!p->time_valid) {
			int modified_time = read_int16(cbuf + offset + 12);
			int modified_date = read_int16(cbuf + offset + 14);
			if (modified_date != 0) {
				p->modified = dostime_to_time(modified_date, modified_time, tz);
				p->time_valid = TRUE;
			}
		}

		p->cs = cs;
		p->offset = read_int32(cbuf + offset + 42);

		offset += CENTRAL_SIZE + filename_len + ext_len + comment_len;
	}
	return cd;

ERROR_END:
	free(cd);
	fs->throw_errorf(mod_zip, "ZipError", "Invalid zipfile format");
	return NULL;
}

/////////////////////////////////////////////////////////////////////////

CentralDirEnd *get_central_dir(Value reader, int size, RefCharset *cs, RefTimeZone *tz)
{
	char *cbuf;
	CentralDirEnd *cdir;
	int offset;
	int tail_size;

	if (size < EOCD_SIZE) {
		return NULL;
	}

	if (size > MAX_ZIP_BUF) {
		// 末尾の64kbを読む
		int seek_to = size - MAX_ZIP_BUF;
		int read_to;
		if (seek_to < 0) {
			seek_to = 0;
		}
		if (!fs->stream_seek_sub(reader, seek_to)) {
			return NULL;
		}
		cbuf = malloc(MAX_ZIP_BUF);
		read_to = MAX_ZIP_BUF;
		if (!fs->stream_read_data(reader, NULL, cbuf, &read_to, FALSE, TRUE)) {
			return NULL;
		}
		tail_size = MAX_ZIP_BUF;
		offset = size - MAX_ZIP_BUF;
	} else {
		int read_to;
		// 全部読む
		if (!fs->stream_seek_sub(reader, 0)) {
			return NULL;
		}
		cbuf = malloc(size);
		read_to = size;
		if (!fs->stream_read_data(reader, NULL, cbuf, &read_to, FALSE, TRUE)) {
			return NULL;
		}
		tail_size = size;
		offset = 0;
	}
	cdir = find_central_dir_end(cbuf, tail_size, offset);
	free(cbuf);

	if (cdir != NULL) {
		CentralDir *cd;
		int read_to;

		cbuf = malloc(cdir->size_of_cdir);
		memset(cbuf, 0, cdir->size_of_cdir);
		if (!fs->stream_seek_sub(reader, cdir->offset_of_cdir)) {
			CentralDirEnd_free(cdir);
			return NULL;
		}
		read_to = cdir->size_of_cdir;
		if (!fs->stream_read_data(reader, NULL, cbuf, &read_to, FALSE, TRUE)) {
			return NULL;
		}
		cd = read_central_dirs(cdir, cbuf, cs, tz);
		free(cbuf);

		if (cd == NULL) {
			CentralDirEnd_free(cdir);
			return NULL;
		}
		cdir->cdir = cd;
		cdir->cs = cs;
		cdir->tz = tz;

		return cdir;
	} else {
		goto FORMAT_ERROR;
	}

FORMAT_ERROR:
	fs->throw_errorf(mod_zip, "ZipError", "Invalid zipfile format");
	return NULL;
}

int get_local_header_size(Value reader, CentralDir *cdir)
{
	char cbuf[LOCAL_SIZE];
	int filename_len, ext_len;
	int read_to = LOCAL_SIZE;

	if (!fs->stream_seek_sub(reader, cdir->pos)) {
		return FALSE;
	}
	if (!fs->stream_read_data(reader, NULL, cbuf, &read_to, FALSE, TRUE)) {
		return FALSE;
	}

	if (memcmp(cbuf, LOCAL_MAGIC, 4) != 0) {
		fs->throw_errorf(mod_zip, "ZipError", "Invalid zipfile format");
		return FALSE;
	}
	filename_len = read_int16(cbuf + 26);
	ext_len = read_int16(cbuf + 28);
	cdir->pos += LOCAL_SIZE + filename_len + ext_len;

	return TRUE;
}

int read_cdir_data(char *dst, int *psize, Value reader, CentralDir *cdir)
{
	int size = *psize;
	int read_to;

	// size_compressedで指定されているサイズまで読み込む
	if (size > cdir->end_pos - cdir->pos) {
		size = cdir->end_pos - cdir->pos;
	}
	if (!fs->stream_seek_sub(reader, cdir->pos)) {
		return FALSE;
	}
	read_to = size;
	if (!fs->stream_read_data(reader, NULL, dst, &read_to, FALSE, TRUE)) {
		return FALSE;
	}

	*psize = read_to;
	cdir->pos += read_to;

	return TRUE;
}

int write_bin_data(CentralDirEnd *cdir, Value writer, const char *p, int size)
{
	const RefNode *type = fs->Value_type(writer);

	if (type == fs->cls_bytesio) {
		RefBytesIO *mb = Value_vp(writer);
		if (!fs->StrBuf_add(&mb->buf, p, size)) {
			return FALSE;
		}
	} else {
		if (!fs->stream_write_data(writer, p, size)) {
			return FALSE;
		}
	}
	cdir->current_offset += size;
	return TRUE;
}
int write_bin_from_stream(CentralDirEnd *cdir, Value stream, Value reader, int src_size)
{
	char *cbuf = malloc(BUFFER_SIZE);
	int read_size = 0;

	while (read_size < src_size) {
		int size_r;
		int size = src_size - read_size;
		if (size > BUFFER_SIZE) {
			size = BUFFER_SIZE;
		}
		size_r = size;
		if (!fs->stream_read_data(reader, NULL, cbuf, &size, FALSE, TRUE)) {
			return FALSE;
		}
		if (!fs->stream_write_data(stream, cbuf, size_r)) {
			return FALSE;
		}
		read_size += size_r;
	}

	free(cbuf);
	cdir->current_offset += src_size;
	return TRUE;
}

int write_local_data(CentralDirEnd *cdir, Value writer, Ref *r)
{
	char local[LOCAL_SIZE];
	CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);
	Ref *zr;
	int offset = cdir->current_offset;

	memcpy(local, LOCAL_MAGIC, 4);
	write_int16(local + 4, 20);         // 要求バージョン
	write_int16(local + 6, cd->flags);  // 汎用フラグ
	write_int16(local + 8, cd->method); // 圧縮メソッド
	if (cd->time_valid) {
		int tm, dt;
		time_to_dostime(&tm, &dt, cd->modified, cdir->tz);
		write_int16(local + 10, tm);
		write_int16(local + 12, dt);
	} else {
		int dt = (1 << 5) | 1;
		int tm = 0;
		write_int16(local + 10, tm);
		write_int16(local + 12, dt);
	}
	write_int32(local + 14, cd->crc32);
	write_int32(local + 18, cd->size_compressed);
	write_int32(local + 22, cd->size);
	write_int16(local + 26, cd->filename.size);
	write_int16(local + 28, 0);        // 拡張フィールド

	if (!write_bin_data(cdir, writer, local, LOCAL_SIZE)) {
		return FALSE;
	}
	if (!write_bin_data(cdir, writer, cd->filename.p, cd->filename.size)) {
		return FALSE;
	}

	zr = Value_vp(r->v[INDEX_ZIPENTRY_REF]);
	if (zr != NULL) {
		// read
		Value reader = zr->v[INDEX_ZIPREADER_READER];
		if (!fs->stream_seek_sub(reader, cd->offset)) {
			return FALSE;
		}
		if (!write_bin_from_stream(cdir, writer, reader, cd->size_compressed)) {
			return FALSE;
		}
	} else {
		// write
		if (!write_bin_data(cdir, writer, cd->data.p, cd->data.size)) {
			return FALSE;
		}
	}
	CentralDirEnd_add(cdir, cd, offset);

	return TRUE;
}

static void make_extra_time(char *p, int64_t tm)
{
	// Unix time
	memcpy(p, "UT", 2);
	write_int16(p + 2, 5);
	p[4] = 3;
	write_int32(p + 5, tm / 1000LL);

	// TODO NTFS time
}
int write_central_dir(CentralDirEnd *cdir, Value writer)
{
	int i;
	enum {
		EXTRA_SIZE = 9,
	};

	cdir->offset_of_cdir = cdir->current_offset;

	for (i = 0; i < cdir->cdir_size; i++) {
		char central[CENTRAL_SIZE];
		char extra[EXTRA_SIZE];
		int extra_size = 0;
		CentralDir *cd = &cdir->cdir[i];

		memcpy(central, "PK\x01\x02", 4);
		write_int16(central + 4, 30);          // 作成されたバージョン
		write_int16(central + 6, 20);          // 要求バージョン
		write_int16(central + 8, cd->flags);   // 汎用フラグ
		write_int16(central + 10, cd->method); // 圧縮メソッド

		if (cd->time_valid) {
			int tm, dt;
			time_to_dostime(&dt, &tm, cd->modified, cdir->tz);
			write_int16(central + 12, tm);
			write_int16(central + 14, dt);
		} else {
			int dt = (1 << 5) | 1;
			int tm = 0;
			write_int16(central + 12, tm);
			write_int16(central + 14, dt);
		}

		if (cd->time_valid) {
			make_extra_time(extra, cd->modified);
			extra_size = EXTRA_SIZE;
		}

		write_int32(central + 16, cd->crc32);
		write_int32(central + 20, cd->size_compressed);
		write_int32(central + 24, cd->size);
		write_int16(central + 28, cd->filename.size);
		write_int16(central + 30, extra_size); // 拡張フィールドの長さ
		write_int16(central + 32, 0);          // ファイルコメントの長さ
		write_int16(central + 34, 0);          // ファイルが開始するディスク番号
		write_int16(central + 36, 0);          // 内部ファイル属性
		write_int32(central + 38, 0);          // 外部ファイル属性
		write_int32(central + 42, cd->offset); // ローカルファイルヘッダの相対オフセット

		if (!write_bin_data(cdir, writer, central, CENTRAL_SIZE)) {
			return FALSE;
		}
		if (!write_bin_data(cdir, writer, cd->filename.p, cd->filename.size)) {
			return FALSE;
		}
		if (cd->time_valid) {
			if (!write_bin_data(cdir, writer, extra, EXTRA_SIZE)) {
				return FALSE;
			}
		}
	}
	cdir->size_of_cdir = cdir->current_offset - cdir->offset_of_cdir;

	return TRUE;
}

int write_end_of_cdir(CentralDirEnd *cdir, Value writer)
{
	char eocd[EOCD_SIZE];

	memcpy(eocd, "PK\x05\x06", 4);
	write_int16(eocd + 4, 0);                // このディスクの数
	write_int16(eocd + 6, 0);                // セントラルディレクトリが開始するディスク
	write_int16(eocd + 8, cdir->cdir_size);  // このディスク上のセントラルディレクトリレコードの数
	write_int16(eocd + 10, cdir->cdir_size); // セントラルディレクトリレコードの合計数
	write_int32(eocd + 12, cdir->size_of_cdir);
	write_int32(eocd + 16, cdir->offset_of_cdir);
	write_int16(eocd + 20, 0);               // コメントの長さ

	if (!write_bin_data(cdir, writer, eocd, EOCD_SIZE)) {
		return FALSE;
	}
	return TRUE;
}


int move_next_file_entry(Value reader)
{
	// PK\003\004 を探す
	RefNode *type = fs->Value_type(reader);
	if (type == fs->cls_bytesio) {
		RefBytesIO *mb = Value_vp(reader);
		StrBuf *sb = &mb->buf;
		int pos;
		int found = FALSE;
		for (pos = mb->cur; pos + 4 <= sb->size; pos++) {
			if (sb->p[pos] == 'P' && memcmp(sb->p + 1, "K\x03\x04", 3) == 0) {
				found = TRUE;
				mb->cur = pos + 4;
				break;
			}
		}
		if (!found) {
			mb->cur = sb->size;
		}
		return found;
	} else {
	}

	return FALSE;
}
