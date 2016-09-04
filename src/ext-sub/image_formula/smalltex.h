#ifndef SMALLTEX_H
#define SMALLTEX_H

#include <stdint.h>



enum {
    MAXEXPRSZ = 32767,      /*max #bytes in input tex expression */
};

typedef struct mathchardef_struct mathchardef_t;

/* -------------------------------------------------------------------------
 * Raster structure (bitmap or bytemap, along with its width and height in bits)
 * -------------------------------------------------------------------------- */
/* --- datatype for pixels --- */
typedef uint8_t pixbyte_t;

typedef struct raster_struct
{
    /* -----------------------------------------------------------------------
    dimensions of raster
    ------------------------------------------------------------------------ */
    int   width;              /* #pixels wide */
    int   height;             /* #pixels high */
    int   format;             /* 1=bitmap, 2=gf/8bits,3=gf/4bits */
    int   pixsz;              /* #bits per pixel, 1 or 8 */
    /* -----------------------------------------------------------------------
    memory for raster
    ------------------------------------------------------------------------ */
    pixbyte_t *pixmap;  /* memory for width*height bits or bytes */
} raster_t;


/* -------------------------------------------------------------------------
subraster_t (bitmap image, its attributes, overlaid position in raster, etc)
-------------------------------------------------------------------------- */
typedef struct subraster_struct
{
    /* --- subraster_t type --- */
    int   type;                   /* charcter or image raster */
    /* --- character info (if subraster_t represents a character) --- */
    const mathchardef_t *symdef;          /* mathchardef identifying image */
    int   baseline;               /*0 if image is entirely descending*/
    int   size;                   /* font size 0-4 */
    /* --- upper-left corner for bitmap (as overlaid on a larger raster) --- */
    int   toprow, leftcol;        /* upper-left corner of subraster_t */
    /* --- pointer to raster bitmap image of subraster_t --- */
    raster_t *image;                /*ptr to bitmap image of subraster_t*/
} subraster_t;



//------------------------------------------------------------------------------
// raster functions

subraster_t *rasterize(const char *expression, int size);
void delete_subraster(subraster_t *sp);


//------------------------------------------------------------------------------
// misc functions

const char *mimeprep(const char *expression);


//------------------------------------------------------------------------------
// Log functions

typedef void error_log_func_t(void *user_data, const char *msg);
void smalltex_setmsg(int newmsglevel, void *user_data, error_log_func_t error_log_func);


#endif /* SMALLTEX_H */
