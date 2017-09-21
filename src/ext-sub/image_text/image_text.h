#ifndef IMAGE_TEXT_INCLUDED
#define IMAGE_TEXT_INCLUDED


#include "fox_io.h"
#include "m_image.h"
#include "m_number.h"

#include <ft2build.h>
#include FT_FREETYPE_H

enum {
    FONT_INDEX_FACE,
    FONT_INDEX_SRC,
    FONT_INDEX_NUM,
};

#ifdef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;
extern const ImageStatic *ist;

extern RefNode *mod_ft;
extern RefNode *mod_image;
extern const RefNode *cls_fileio;
extern RefNode *cls_ftface;
extern RefNode *cls_color;

#ifdef DEFINE_GLOBALS
#undef extern
#endif


#define FLOAT_TO_26_6(f) ((long)((f) * 64.0))
#define Value_ftface(val) ((FT_Face)(Value_ptr(Value_ref(val)->v[FONT_INDEX_FACE])))


// ft.c
void ft_initialize(void);
int ft_open_face_stream(Ref *r, int font_index);

int ftfont_close(Value *vret, Value *v, RefNode *node);
int ftfont_get_attr(Value *vret, Value *v, RefNode *node);
int ftfont_family_name(Value *vret, Value *v, RefNode *node);
int ftfont_to_str(Value *vret, Value *v, RefNode *node);
int ft_quit(Value *vret, Value *v, RefNode *node);

void get_glyph_indexes(StrBuf *indexes, const char *p, const char *end, FT_Face *a_face);
int get_char_positions(StrBuf *offsets, const uint32_t *indexes, FT_Face *a_face, long face_size, long *pcx);


#endif /* IMAGE_TEXT_INCLUDED */
