#define DEFINE_GLOBALS
#include "m_image.h"
#include "mmtypes.h"
#include <stdlib.h>
#include <string.h>


const char *mimeprep(const char *expression);
subraster *rasterize(const char *expression, int size);
int delete_subraster(subraster *sp);


static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *cls_image;
static RefNode *mod_image;
static RefNode *mod_formula;


#define GET_BIT(src, i) (src[(i) / 8] & (1 << ((i) % 8)))

static void raster_to_image(RefImage *image, raster *rs)
{
    int size = image->width * image->height;
    const uint8_t *src = (const uint8_t *)rs->pixmap;
    uint8_t *dst = image->data;
    int i;

    if (rs->pixsz == 1) {
        for (i = 0; i < size; i++) {
            dst[i] = (GET_BIT(src, i) ? 0 : 255);
        }
    } else {
        for (i = 0; i < size; i++) {
            dst[i] = 255 - src[i];
        }
    }
}

static uint32_t *create_palette(uint32_t fgcolor, uint32_t bgcolor)
{
    int i, j;
    uint32_t *pal = malloc(sizeof(uint32_t) * PALETTE_NUM);
    memset(pal, 0, sizeof(uint32_t) * PALETTE_NUM);

    if (fgcolor == 0) {
        fgcolor = (fgcolor & COLOR_A_MASK) | (bgcolor & ~COLOR_A_MASK);
    } else if (bgcolor == 0) {
        bgcolor = (bgcolor & COLOR_A_MASK) | (fgcolor & ~COLOR_A_MASK);
    }

    for (i = 0; i < 4; i++) {
        uint32_t mask = 0xFF << (i * 8);
        uint32_t imask = ~mask;
        int fgcol = (fgcolor & mask) >> (i * 8);
        int bgcol = (bgcolor & mask) >> (i * 8);

        for (j = 0; j < PALETTE_NUM; j++) {
            int v = fgcol + (bgcol - fgcol) * j / 255;
            pal[j] = (pal[j] & imask) | (v << (i * 8));
        }
    }
    return pal;
}

static int mimetex_mimetex(Value *vret, Value *v, RefNode *node)
{
    RefStr *r_expr = Value_vp(v[1]);
    char *c_expr;
    RefImage *image;
    int mag = 1;
    int is_palette = FALSE;
    uint32_t fgcolor = COLOR_A_MASK; // black
    uint32_t bgcolor = 0;
    raster *rs;
    subraster *sp;

    if (fg->stk_top > v + 2) {
        mag = fs->Value_int64(v[2], NULL);
        if (mag < 1 || mag > 64) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Argument #2 must be 2..64");
            return FALSE;
        }
    }
    if (fg->stk_top > v + 3) {
        fgcolor = Value_integral(v[3]);
        is_palette = TRUE;
    }
    if (fg->stk_top > v + 4) {
        bgcolor = Value_integral(v[4]);
    }

    c_expr = malloc(MAXEXPRSZ + 1);
    if (r_expr->size <= MAXEXPRSZ) {
        memcpy(c_expr, r_expr->c, r_expr->size);
        c_expr[r_expr->size] = '\0';
    } else {
        memcpy(c_expr, r_expr->c, MAXEXPRSZ);
        c_expr[MAXEXPRSZ] = '\0';
    }
    sp = rasterize(r_expr->c, mag);

    if (sp == NULL || sp->image == NULL) {
        goto ERROR_END;
    }
    rs = sp->image;
    image = fs->buf_new(cls_image, sizeof(RefImage));
    *vret = vp_Value(image);

    image->bands = (is_palette ? BAND_P : BAND_L);
    image->width = rs->width;
    image->height = rs->height;

    if (is_palette) {
        image->palette = create_palette(fgcolor, bgcolor);
    }
    image->data = malloc(image->width * image->height);
    image->pitch = image->width;

    raster_to_image(image, rs);
    free(c_expr);

    return TRUE;

ERROR_END:
    if (sp != NULL) {
        delete_subraster(sp);
    }
    free(c_expr);
    fs->throw_errorf(mod_formula, "MimetexError", "mimeTeX failed to render your expression");
    return FALSE;
}

static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "mimetex", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, mimetex_mimetex, 1, 4, (void*)FALSE, fs->cls_str, fs->cls_int, NULL, NULL);
    fs->add_unresolved_args(m, mod_image, "Color", n, 2);
    fs->add_unresolved_args(m, mod_image, "Color", n, 3);

    n = fs->define_identifier(m, m, "mimetex_aa", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, mimetex_mimetex, 1, 4, (void*)TRUE, fs->cls_str, fs->cls_int, NULL, NULL);
    fs->add_unresolved_args(m, mod_image, "Color", n, 2);
    fs->add_unresolved_args(m, mod_image, "Color", n, 3);
}
static void define_class(RefNode *m)
{
    RefNode *cls;

    cls = fs->define_identifier(m, m, "MimetexError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_formula = m;

    mod_image = fs->get_module_by_name("image", -1, TRUE, FALSE);
    fs->add_unresolved_ptr(m, mod_image, "Image", &cls_image);
    define_class(m);
    define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    return "Build at\t" __DATE__ "\n";
}
