#ifndef _IMAGE_RAW_H_
#define _IMAGE_RAW_H_

#include "fox.h"
#include "m_image.h"


enum {
	TYPE_NONE,
	TYPE_PBM_ASCII,
	TYPE_PGM_ASCII,
	TYPE_PPM_ASCII,
	TYPE_PBM_BIN,
	TYPE_PGM_BIN,
	TYPE_PPM_BIN,
};

#ifndef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;
extern RefNode *mod_raw;
extern RefNode *mod_image;

#ifndef DEFINE_GLOBALS
#undef extern
#endif

int load_image_bmp_sub(RefImage *image, Value r, int info_only);
int save_image_bmp_sub(RefImage *image, Value w);

int load_image_pnm_sub(RefImage *image, Value r, int info_only);
int save_image_pnm_sub(RefImage *image, Value w, int type, int threshold);


#endif /* _IMAGE_RAW_H_ */
