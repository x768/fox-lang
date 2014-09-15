#ifndef _MMTYPES_H_
#define _MMTYPES_H_

//#define SS
//#define SSFONTS
#define TEXFONTS


/* ---
 * additional font attributes (only size is implemented)
 * ----------------------------------------------------- */
/* --- font sizes 0-10 = tiny=0,scriptsize=1,footnotesize=2,small=3,
       normalsize=4,large=5,Large=6,LARGE=7,huge=8,Huge=9,HUGE=10 --- */
/* --- (mimeTeX adds HUGE) --- */
#define LARGESTSIZE (10)
#ifdef DEFAULTSIZE
  #ifndef NORMALSIZE
    #define NORMALSIZE (DEFAULTSIZE)
  #endif
#endif
#ifndef NORMALSIZE
  /*#define NORMALSIZE  (2)*/
  /*#define NORMALSIZE  (3)*/
  #define NORMALSIZE    (4)
#endif
#ifndef DISPLAYSIZE
  /* --- automatically sets scripts in \displaystyle when fontsize>= --- */
  /*#define DISPLAYSIZE (NORMALSIZE+1)*/
  /*#define DISPLAYSIZE (3)*/
  #define DISPLAYSIZE   (4)
#endif

/* --- families for mathchardef (TeXbook, top of pg.431) --- */
#define CMR10       (1)     /* normal roman */
#define CMMI10      (2)     /* math italic */
#define CMMIB10     (3)     /* math italic bold */
#define CMSY10      (4)     /* math symbol */
#define CMEX10      (5)     /* math extension */
#define RSFS10      (6)     /* rsfs \scrA ... \scrZ */
#define BBOLD10     (7)     /* blackboard bold \mathbb A ... */
#define STMARY10    (8)     /* stmaryrd math symbols */
#define CYR10       (9)     /* cyrillic (wncyr10.mf) */
#define CMMI10GR    (10)        /* CMMI10 with a for \alpha, etc */
#define CMMI10BGR   (11)        /* CMMIB10 with a for \alpha, etc */
#define BBOLD10GR   (12)        /* BBOLD10 with a for \alpha, etc */
#define NOTACHAR    (99)        /* e.g., \frac */


/* ---
 * classes for mathchardef (TeXbook pg.154)
 * ---------------------------------------- */
#define ORDINARY    (0)     /* e.g., /    */
#define OPERATOR    (1)     /* e.g., \sum */
#define BINARYOP    (2)     /* e.g., +    */
#define RELATION    (3)     /* e.g., =    */
#define OPENING     (4)     /* e.g., (    */
#define CLOSING     (5)     /* e.g., }    */
#define PUNCTION    (6)     /* e.g., , (punctuation) */
#define VARIABLE    (7)     /* e.g., x    */
#define DISPOPER    (8)     /* e.g., Bigint (displaymath opers)*/
#define SPACEOPER   (9)     /* e.g., \hspace{} */
#define MAXCLASS    (9)     /* just for index checks */
#define UPPERBIG    DISPOPER    /*how to interpret Bigxxx operators*/
#define LOWERBIG    DISPOPER    /*how to interpret bigxxx operators*/
/* --- class aliases --- */
#define ARROW       RELATION
/* --- dummy argument value for handlers --- */
#define NOVALUE     (-989898)   /*charnum,family,class used as args*/


/* ---
 * internal buffer sizes
 * --------------------- */
#define MAXEXPRSZ (32768-1)     /*max #bytes in input tex expression */
#define MAXSUBXSZ (((MAXEXPRSZ+1)/2)-1) /*max #bytes in input subexpression */
#define MAXTOKNSZ (((MAXSUBXSZ+1)/4)-1) /* max #bytes in input token */
#define MAXLINESZ (4096-1)      /* max #chars in line from file */



/* -------------------------------------------------------------------------
Raster structure (bitmap or bytemap, along with its width and height in bits)
-------------------------------------------------------------------------- */
/* --- 8-bit datatype (always unsigned) --- */
typedef unsigned char intbyte;
/* --- datatype for pixels --- */
typedef char pixbyte;

typedef struct subraster_struct subraster;

typedef subraster *(*HANDLER)(const char **expression, int size, subraster *basesp, int flag, int value, int arg3);

/* --- raster structure --- */
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
  pixbyte *pixmap;  /* memory for width*height bits or bytes */
} raster;

/* -------------------------------------------------------------------------
character definition struct (font info from .gf file describing a char)
-------------------------------------------------------------------------- */
typedef struct chardef_struct
{
  /* -----------------------------------------------------------------------
  character description
  ------------------------------------------------------------------------ */
  /* --- character identification as given in .gf font file --- */
  int   charnum;                /*different gf files resuse same num*/
  int   location;               /* location in font */
  /* --- upper-left and lower-left corners of char (topcol=botcol?) --- */
  int   toprow, topleftcol;     /* upper-left corner */
  int   botrow, botleftcol;     /* lower-left corner */
  /* -----------------------------------------------------------------------
  character bitmap raster (image.width is character width, ditto height)
  ------------------------------------------------------------------------ */
  raster image;                 /* bitmap image of character */
} chardef;


/* -------------------------------------------------------------------------
Font info corresponding to TeX \matchardef, see TeXbook Appendix F (page 431)
-------------------------------------------------------------------------- */
typedef struct mathchardef_struct
{
  /* -----------------------------------------------------------------------
  symbol name ("a", "\alpha", "1", etc)
  ------------------------------------------------------------------------ */
  const char *symbol;           /* as it appears in a source file */
  /* -----------------------------------------------------------------------
  components of \mathchardef hexadecimal code assigned to this symbol
  ------------------------------------------------------------------------ */
  int   charnum;                /* char# (as given in .gf file) */
  int   family;                 /* font family e.g., 2=math symbol */
  int   class;                  /* e.g., 3=relation, TexBook pg.154*/
  /* ------------------------------------------------------------------------
  Extra info: some math "functions" require special processing (e.g., \frac)
  ------------------------------------------------------------------------ */
  /* --- function that performs special processing required by symbol --- */
  HANDLER handler;              /* e.g., rastfrac() for \frac's */
} mathchardef;


/* -------------------------------------------------------------------------
subraster (bitmap image, its attributes, overlaid position in raster, etc)
-------------------------------------------------------------------------- */
struct subraster_struct
{
  /* --- subraster type --- */
  int   type;                   /* charcter or image raster */
  /* --- character info (if subraster represents a character) --- */
  mathchardef *symdef;          /* mathchardef identifying image */
  int   baseline;               /*0 if image is entirely descending*/
  int   size;                   /* font size 0-4 */
  /* --- upper-left corner for bitmap (as overlaid on a larger raster) --- */
  int   toprow, leftcol;        /* upper-left corner of subraster */
  /* --- pointer to raster bitmap image of subraster --- */
  raster *image;                /*ptr to bitmap image of subraster*/
};

typedef struct store_struct
{
  const char *identifier;       /* identifier */
  int *value;                   /* address of corresponding value */
} STORE;


typedef struct aaparameters_struct
{
    int centerwt;               /* lowpass matrix center   pixel wt*/
    int adjacentwt;             /* lowpass matrix adjacent pixel wt*/
    int cornerwt;               /* lowpass matrix corner   pixel wt*/
    int minadjacent;            /* darken if >= adjacent pts black */
    int maxadjacent;            /* darken if <= adjacent pts black */
    int fgalias, fgonly;        /* aapnm() params */
    int bgalias, bgonly;        /* aapnm() params */
} aaparameters;


/* -------------------------------------------------------------------------
font family
-------------------------------------------------------------------------- */

typedef struct fontfamily_struct /* typedef for fontfamily */
{
  /* -----------------------------------------------------------------------
  several sizes, fontdef[0-7]=tiny,small,normal,large,Large,LARGE,huge,HUGE
  ------------------------------------------------------------------------ */
  int   family;             /* font family e.g., 2=math symbol */
  chardef *fontdef[LARGESTSIZE+2];  /*small=(fontdef[1])[charnum].image*/
} fontfamily;

#endif /* _MMTYPES_H_ */
