#ifndef _ZIPFILE_H_
#define _ZIPFILE_H_

#include "fox.h"
#include "fox_io.h"
#include <zlib.h>


enum {
	CDIR_FLAG_CRYPT = 1,
	CDIR_FLAG_PK0708 = (1 << 2),
	CDIR_FLAG_UTF8 = (1 << 11),

	MAX_ALLOC = 16 * 1024 * 1024,
};
enum {
	INDEX_Z_STREAM = INDEX_STREAM_NUM,
	INDEX_Z_IN,
	INDEX_Z_OUT,
	INDEX_Z_IN_BUF,
	INDEX_Z_LEVEL,
	INDEX_Z_NUM,
};
enum {
	INDEX_ZIPREADER_READER,
	INDEX_ZIPREADER_CDIR,
	INDEX_ZIPREADER_NUM,
};
enum {
	INDEX_ZIPWRITER_DEST,
	INDEX_ZIPWRITER_CDIR,
	INDEX_ZIPWRITER_NUM,
};
enum {
	INDEX_ZIPENTRY_REF = INDEX_STREAM_NUM,
	INDEX_ZIPENTRY_CDIR,
	INDEX_ZIPENTRY_IN_BUF,
	INDEX_ZIPENTRY_IN_BUF_SIZE,
	INDEX_ZIPENTRY_NUM,
};
enum {
	INDEX_ZIPENTRYITER_REF,
	INDEX_ZIPENTRYITER_INDEX,
	INDEX_ZIPENTRYITER_NUM,
};

/* compression methods */
#define ZIP_CM_DEFAULT	      -1  /* better of deflate or store */
#define ZIP_CM_STORE	       0  /* stored (uncompressed) */
#define ZIP_CM_SHRINK	       1  /* shrunk */
#define ZIP_CM_REDUCE_1	       2  /* reduced with factor 1 */
#define ZIP_CM_REDUCE_2	       3  /* reduced with factor 2 */
#define ZIP_CM_REDUCE_3	       4  /* reduced with factor 3 */
#define ZIP_CM_REDUCE_4	       5  /* reduced with factor 4 */
#define ZIP_CM_IMPLODE	       6  /* imploded */
/* 7 - Reserved for Tokenizing compression algorithm */
#define ZIP_CM_DEFLATE	       8  /* deflated */
#define ZIP_CM_DEFLATE64       9  /* deflate64 */
#define ZIP_CM_PKWARE_IMPLODE 10  /* PKWARE imploding */
/* 11 - Reserved by PKWARE */
#define ZIP_CM_BZIP2          12  /* compressed using BZIP2 algorithm */
/* 13 - Reserved by PKWARE */
#define ZIP_CM_LZMA	      14  /* LZMA (EFS) */
/* 15-17 - Reserved by PKWARE */
#define ZIP_CM_TERSE	      18  /* compressed using IBM TERSE (new) */
#define ZIP_CM_LZ77           19  /* IBM LZ77 z Architecture (PFS) */
#define ZIP_CM_WAVPACK	      97  /* WavPack compressed data */
#define ZIP_CM_PPMD	      98  /* PPMd version I, Rev 1 */


/*
 0 	4 	ローカルファイルヘッダのシグネチャ = 0x04034b50（ビッグエンディアンでPK\003\004）
 4 	2 	展開に必要なバージョン (最小バージョン)
 6 	2 	汎用目的のビットフラグ
 8 	2 	圧縮メソッド
10 	2 	ファイルの最終変更時間
12 	2 	ファイルの最終変更日付
14 	4 	CRC-32
18 	4 	圧縮サイズ
22 	4 	非圧縮サイズ
26 	2 	ファイル名の長さ (n)
28 	2 	拡張フィールドの長さ (m)
30 	n 	ファイル名
30+n 	m 	拡張フィールド
 */

/*
 0 	4 	セントラルディレクトリファイルヘッダのシグネチャ = 0x02014b50（ビッグエンディアンでPK\001\002）
 4 	2 	作成されたバージョン
 6 	2 	展開に必要なバージョン (最小バージョン)
 8 	2 	汎用目的のビットフラグ
10 	2 	圧縮メソッド
12 	2 	ファイルの最終変更時間
14 	2 	ファイルの最終変更日付
16 	4 	CRC-32
20 	4 	圧縮サイズ
24 	4 	非圧縮サイズ
28 	2 	ファイル名の長さ (n)
30 	2 	拡張フィールドの長さ (m)
32 	2 	ファイルコメントの長さ (k)
34 	2 	ファイルが開始するディスク番号
36 	2 	内部ファイル属性
38 	4 	外部ファイル属性
42 	4 	ローカルファイルヘッダの相対オフセット
46 	n 	ファイル名
46+n 	m 	拡張フィールド
46+n+m 	k 	ファイルコメント
 */
typedef struct {
	RefCharset *cs;
	int pos;
	int end_pos;

	uint16_t flags;
	uint16_t method;
	uint32_t crc32;
	uint32_t size_compressed;
	uint32_t size;
	uint16_t attr1;
	uint32_t attr2;
	int32_t offset;

	int64_t modified;
	int time_valid;
	uint32_t level;   // 0-9
	StrBuf filename;
	StrBuf data;      // データを作成する時に使う

	int z_init;
	int z_finish;
	z_stream z;
} CentralDir;

/*
 0 	4 	セントラルディレクトリの終端レコードのシグネチャ = 0x06054b50（ビッグエンディアンでPK\005\006）
 4 	2 	このディスクの数
 6 	2 	セントラルディレクトリが開始するディスク
 8 	2 	このディスク上のセントラルディレクトリレコードの数
10 	2 	セントラルディレクトリレコードの合計数
12 	4 	セントラルディレクトリのサイズ (バイト)
16 	4 	セントラルディレクトリの開始位置のオフセット
20 	2 	ZIP ファイルのコメントの長さ (n)
22 	n 	ZIP ファイルのコメント
 */
typedef struct {
	uint32_t size_of_cdir;
	uint32_t offset_of_cdir;

	RefCharset *cs;
	RefTimeZone *tz;

	int current_offset;       // ZipWriterで使用
	int cdir_size, cdir_max;
	CentralDir *cdir;
} CentralDirEnd;




#ifdef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;
extern RefNode *mod_zip;

#ifdef DEFINE_GLOBALS
#undef extern
#endif


void CentralDirEnd_free(CentralDirEnd *cdir);
CentralDir *find_central_dir(Value reader);
CentralDirEnd *get_central_dir(Value reader, int size, RefCharset *cs, RefTimeZone *tz);
int get_local_header_size(Value reader, CentralDir *cdir);
int read_cdir_data(char *dst, int *psize, Value reader, CentralDir *cdir);
int write_bin_data(CentralDirEnd *cdir, Value writer, const char *p, int size);
int write_local_data(CentralDirEnd *cdir, Value writer, Ref *r);
int write_central_dir(CentralDirEnd *cdir, Value writer);
int write_end_of_cdir(CentralDirEnd *cdir, Value writer);

int move_next_file_entry(Value reader);


#endif /* _ZIPFILE_H_ */
