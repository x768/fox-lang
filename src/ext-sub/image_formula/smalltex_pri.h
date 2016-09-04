#ifndef SMALLTEX_PRI_H
#define SMALLTEX_PRI_H
/****************************************************************************
 *
 * Copyright(c) 2002-2011, John Forkosh Associates, Inc. All rights reserved.
 *           http://www.forkosh.com   mailto: john@forkosh.com
 * --------------------------------------------------------------------------
 * This file is part of mimeTeX, which is free software. You may redistribute
 * and/or modify it under the terms of the GNU General Public License,
 * version 3 or later, as published by the Free Software Foundation.
 *      MimeTeX is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY, not even the implied warranty of MERCHANTABILITY.
 * See the GNU General Public License for specific details.
 *      By using mimeTeX, you warrant that you have read, understood and
 * agreed to these terms and conditions, and that you possess the legal
 * right and ability to enter into this agreement and to use mimeTeX
 * in accordance with it.
 *      Your mimetex.zip distribution file should contain the file COPYING,
 * an ascii text copy of the GNU General Public License, version 3.
 * If not, point your browser to  http://www.gnu.org/licenses/
 * or write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,  Boston, MA 02111-1307 USA.
 * --------------------------------------------------------------------------
 *
 * Purpose: Structures, macros, symbols,
 *      and static font data for mimetex (and friends)
 * 
 * Source:  mimetex.h
 *
 * Notes:     o #define TEXFONTS before #include "mimetex.h"
 *      if you need the fonttable[] (of fontfamily's) set up.
 *      mimetex.c needs this; other modules probably don't
 *      because they'll call access functions from mimetex.c
 *      that hide the underlying font data
 *
 * --------------------------------------------------------------------------
 * Revision History:
 * 09/18/02 J.Forkosh   Installation.
 * 12/11/02 J.Forkosh   Version 1.00 released.
 * 07/04/03 J.Forkosh   Version 1.01 released.
 * ---
 * 09/06/08 J.Forkosh   Version 1.70 released.
 *
 ***************************************************************************/


#include "smalltex.h"



#ifndef DEBUG_LOG_LEVEL
#define DEBUG_LOG_LEVEL 0
#endif

#if DEBUG_LOG_LEVEL > 0
#include <stdio.h>
#endif

#define TEXFONTS

/* ---
 * check for supersampling or low-pass anti-aliasing
 * ------------------------------------------------- */
#ifdef SS
    #define ISSUPERSAMPLING 1
    #ifndef AAALGORITHM
        #define AAALGORITHM 1           /* default supersampling algorithm */
    #endif
    #ifndef AA                      /* anti-aliasing not explicitly set */
        #define AA                      /* so define it ourselves */
    #endif
    #ifndef SSFONTS                 /* need supersampling fonts */
        #define SSFONTS
    #endif
#else
    #define ISSUPERSAMPLING 0
    #ifndef AAALGORITHM
        #define AAALGORITHM 3 /*2*/     /* default lowpass algorithm */
    #endif
#endif

#ifndef MAXFOLLOW
    #define MAXFOLLOW 8             /* aafollowline() maxturn default */
#endif

#ifndef ISSUPERSAMPLING
#define ISSUPERSAMPLING 0
#endif

/* ---
 * anti-aliasing parameters
 * ------------------------ */
#ifndef CENTERWT
/*#define CENTERWT 32 *//* anti-aliasing centerwt default */
/*#define CENTERWT 10 *//* anti-aliasing centerwt default */
#define CENTERWT 8              /* anti-aliasing centerwt default */
#endif
#ifndef ADJACENTWT
/*#define ADJACENTWT 3 *//* anti-aliasing adjacentwt default */
#define ADJACENTWT 4            /* anti-aliasing adjacentwt default */
#endif
#ifndef CORNERWT
#define CORNERWT 1              /* anti-aliasing cornerwt default */
#endif
#ifndef MINADJACENT
#define MINADJACENT 6           /*anti-aliasing minadjacent default */
#endif
#ifndef MAXADJACENT
#define MAXADJACENT 8           /*anti-aliasing maxadjacent default */
#endif

/* ---
 * classes for mathchardef (TeXbook pg.154)
 * ---------------------------------------- */
enum {
    ORDINARY  = 0,          /* e.g., /    */
    OPERATOR  = 1,          /* e.g., \sum */
    BINARYOP  = 2,          /* e.g., +    */
    RELATION  = 3,          /* e.g., =    */
    OPENING   = 4,          /* e.g., (    */
    CLOSING   = 5,          /* e.g., }    */
    PUNCTION  = 6,          /* e.g., , (punctuation) */
    VARIABLE  = 7,          /* e.g., x    */
    DISPOPER  = 8,          /* e.g., Bigint (displaymath opers)*/
    SPACEOPER = 9,          /* e.g., \hspace{} */
    MAXCLASS  = 9,          /* just for index checks */
    UPPERBIG  = DISPOPER,   /*how to interpret Bigxxx operators*/
    LOWERBIG  = DISPOPER,   /*how to interpret bigxxx operators*/
/* --- class aliases --- */
    ARROW     = RELATION,
/* --- dummy argument value for handlers --- */
    NOVALUE   = -989898,    /*charnum,family,class used as args*/
};

//------------------------------------------------------------------------------
/* --- families for mathchardef (TeXbook, top of pg.431) --- */
enum {
    CMR10      = 1,     /* normal roman */
    CMMI10     = 2,     /* math italic */
    CMMIB10    = 3,     /* math italic bold */
    CMSY10     = 4,     /* math symbol */
    CMEX10     = 5,     /* math extension */
    RSFS10     = 6,     /* rsfs \scrA ... \scrZ */
    BBOLD10    = 7,     /* blackboard bold \mathbb A ... */
    STMARY10   = 8,     /* stmaryrd math symbols */
    CYR10      = 9,     /* cyrillic (wncyr10.mf) */
    CMMI10GR   = 10,    /* CMMI10 with a for \alpha, etc */
    CMMI10BGR  = 11,    /* CMMIB10 with a for \alpha, etc */
    BBOLD10GR  = 12,    /* BBOLD10 with a for \alpha, etc */
    NOTACHAR   = 99,    /* e.g., \frac */
};

enum {
    /* --- sqrt --- */
    SQRTACCENT     = 1,         /* \sqrt */
    /* --- accents --- */
    BARACCENT      = 11,        /* \bar \overline */
    UNDERBARACCENT = 12,        /* \underline */
    HATACCENT      = 13,        /* \hat */
    DOTACCENT      = 14,        /* \dot */
    DDOTACCENT     = 15,        /* \ddot */
    VECACCENT      = 16,        /* \vec */
    TILDEACCENT    = 17,        /* \tilde */
    OVERBRACE      = 18,        /* \overbrace */
    UNDERBRACE     = 19,        /* \underbrace */
    /* --- flags/modes --- */
    ISFONTFAM      = 1,         /* set font family */
    ISDISPLAYSTYLE = 2,         /* set isdisplaystyle */
    ISDISPLAYSIZE  = 21,        /* set displaysize */
    ISFONTSIZE     = 3,         /* set fontsize */
    ISMAGSTEP      = 31,        /* set magstep */
    ISWEIGHT       = 4,         /* set aa params */
    ISSUPER        = 6,         /* set supersampling/lowpass */
    ISAAALGORITHM  = 61,        /* set anti-aliasing algorithm */
    ISCENTERWT     = 62,        /* set anti-aliasing center weight */
    ISADJACENTWT   = 63,        /* set anti-aliasing adjacent weight */
    ISCORNERWT     = 64,        /* set anti-aliasing adjacent weight */
    ISSHRINK       = 7,         /* set supersampling shrinkfactor */
    UNITLENGTH     = 8,         /* set unitlength */
    ISREVERSE      = 10,        /* set reverse video colors */
    ISSMASH        = 12,        /* set (minimum) "smash" margin */

    BLANKSIGNAL    = -991234,   /*rastsmash signal right-hand blank */
};

/* ---
 * internal buffer sizes
 * --------------------- */
enum {
    MAXSUBXSZ = (MAXEXPRSZ+1)/2 - 1,    /*max #bytes in input subexpression */
    MAXTOKNSZ = (MAXSUBXSZ+1)/4 - 1,    /* max #bytes in input token */
    MAXLINESZ = 4096-1,                 /* max #chars in line from file */
};

/* ---
 * other parameters
 * ------------------------ */
enum {
    MAXSTORE    = 6,                /* max identifiers */
    SMASHMARGIN = 0,
    SMASHCHECK  = 0,
    TEXTWIDTH   = 400,
    CMSYEX      = 109,              /*select CMSY10, CMEX10 or STMARY10 */
    LARGESTSIZE = 10,
    NORMALSIZE  = 4,
    DISPLAYSIZE = 4,
};

/* -------------------------------------------------------------------------
miscellaneous macros
-------------------------------------------------------------------------- */
#define compress(s,c) if((s)!=NULL) /* remove embedded c's from s */ \
    { char *p; while((p=strchr((s),(c)))!=NULL) {strsqueeze(p,1);} } else
#define slower(s)  if ((s)!=NULL)   /* lowercase all chars in s */ \
    { char *p=(s); while(*p!='\0'){*p=tolower(*p); p++;} } else
                                                                /*subraster_t *subrastcpy(); *//* need global module declaration */
/*#define spnosmash(sp) if (sp->type==CHARASTER) sp=subrastcpy(sp); \ */
/*  sp->type=blanksignal */
/* ---evaluate \directive[arg] or \directive{arg} scaled by unitlength--- */
#define eround(arg) (iround(unitlength*((double)evalterm(mimestore,(arg)))))
/* --- check if a string is empty --- */
#define isempty(s)  ((s)==NULL?1:(*(s)=='\0'?1:0))
/* --- last char of a string --- */
#define lastchar(s) (isempty(s)?'\0':*((s)+(strlen(s)-1)))
/* --- lowercase a string --- */
#define strlower(s) strnlower((s),0)    /* lowercase an entire string */
/* --- strip leading and trailing whitespace (including ~) --- */
#define trimwhite(thisstr) if ( (thisstr) != NULL ) { \
    int thislen_m = strlen(thisstr); \
    while ( --thislen_m >= 0 ) \
      if ( isthischar((thisstr)[thislen_m]," \t\n\r\f\v") ) \
        (thisstr)[thislen_m] = '\0'; \
      else break; \
    if ( (thislen_m = strspn((thisstr)," \t\n\r\f\v")) > 0 ) \
      {strsqueeze((thisstr),thislen_m);} } else
/* --- strncpy() n bytes and make sure it's null-terminated --- */
#define strninit(target,source,n) if( (target)!=NULL && (n)>=0 ) { \
      const char *thissource = (source); \
      (target)[0] = '\0'; \
      if ( (n)>0 && thissource!=NULL ) { \
        strncpy((target),thissource,(n)); \
        (target)[(n)] = '\0'; } }
/* --- strcpy(s,s+n) using memmove() (also works for negative n) --- */
#define strsqueeze(s,n) if((n)!=0) { if(!isempty((s))) { \
    int thislen3=strlen(s); \
    if ((n) >= thislen3) *(s) = '\0'; \
    else memmove(s,s+(n),1+thislen3-(n)); }} else       /*user supplies final; */
/* --- strsqueeze(s,t) with two pointers --- */
#define strsqueezep(s,t) if(!isempty((s))&&!isempty((t))) { \
    int sqlen=strlen((s))-strlen((t)); \
    if (sqlen>0 && sqlen<=999) {strsqueeze((s),sqlen);} } else


/* -------------------------------------------------------------------------
Font info corresponding to TeX \matchardef, see TeXbook Appendix F (page 431)
-------------------------------------------------------------------------- */

struct mathchardef_struct
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
    /* e.g., rastfrac() for \frac's */
    subraster_t *(*handler)(const char **expression, int size,
                            const subraster_t *basesp, int flag, int value, int arg3);
};

/* -------------------------------------------------------------------------
character definition struct (font info from .gf file describing a char)
-------------------------------------------------------------------------- */
typedef struct
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
    raster_t image;                 /* bitmap image of character */
} chardef_t;


typedef struct
{
    const char *identifier;       /* identifier */
    int *value;                   /* address of corresponding value */
} store_t;


typedef struct
{
    int centerwt;               /* lowpass matrix center   pixel wt*/
    int adjacentwt;             /* lowpass matrix adjacent pixel wt*/
    int cornerwt;               /* lowpass matrix corner   pixel wt*/
    int minadjacent;            /* darken if >= adjacent pts black */
    int maxadjacent;            /* darken if <= adjacent pts black */
    int fgalias, fgonly;        /* aapnm() params */
    int bgalias, bgonly;        /* aapnm() params */
} aaparameters_t;

typedef struct {
    const char *name;
    int family;
    int istext;
    int class;
} fontinfo_t;

/* -------------------------------------------------------------------------
font family
-------------------------------------------------------------------------- */
typedef struct /* typedef for fontfamily */
{
    /* -----------------------------------------------------------------------
    several sizes, fontdef[0-7]=tiny,small,normal,large,Large,LARGE,huge,HUGE
    ------------------------------------------------------------------------ */
    int   family;             /* font family e.g., 2=math symbol */
    chardef_t *fontdef[LARGESTSIZE+2];  /*small=(fontdef[1])[charnum].image*/
} fontfamily_t;

//------------------------------------------------------------------------------
// global data variable

extern const mathchardef_t symtable[];
extern const store_t mimestore[];
extern const fontinfo_t fontinfo[];
extern const int shrinkfactors[];
extern const int symspace[11][11];
extern fontfamily_t aafonttable[];
extern fontfamily_t ssfonttable[];

//------------------------------------------------------------------------------
// global variable

extern int daemonlevel;         /* incremented in main() */
extern int recurlevel;          /* inc/decremented in rasterize() */
extern int scriptlevel;         /* inc/decremented in rastlimits() */
extern int isligature;          /* true if ligature found */
extern const char *subexprptr;  /* ptr within expression to subexpr */

extern int isdisplaystyle;      /* displaystyle mode (forced if 2) */
extern int ispreambledollars;   /* displaystyle mode set by $$...$$ */
extern int fontnum;             /* cal=1,scr=2,rm=3,it=4,bb=5,bf=6 */
extern int fontsize;            /* current size */
extern int magstep;             /* magstep (1=no change) */
extern int displaysize;         /* use \displaystyle when fontsize>= */
extern int shrinkfactor;        /* shrinkfactors[fontsize] */
extern int rastlift;            /* rastraise() lift parameter */
extern int rastlift1;           /* rastraise() lift for base exprssn */
extern double unitlength;       /* #pixels per unit (may be <1.0) */
extern int iunitlength;         /* #pixels per unit as int for store */
extern int isdisplaystyle;      /* displaystyle mode (forced if 2) */
extern int ispreambledollars;   /* displaystyle mode set by $$...$$ */
extern int fontnum;             /* cal=1,scr=2,rm=3,it=4,bb=5,bf=6 */
extern int fontsize;            /* current size */
extern int magstep;             /* magstep (1=no change) */
extern int displaysize;         /* use \displaystyle when fontsize>= */
extern int shrinkfactor;        /* shrinkfactors[fontsize] */
extern int rastlift;            /* rastraise() lift parameter */
extern int rastlift1;           /* rastraise() lift for base exprssn */
extern double unitlength;       /* #pixels per unit (may be <1.0) */
extern int iunitlength;         /* #pixels per unit as int for store */
extern int isnocatspace;        /* >0 to not add space in rastcat() */
extern int smashmargin;         /* minimum "smash" margin */
extern int mathsmashmargin;     /* needed for \text{if $n-m$ even} */
extern int issmashdelta;        /* true if smashmargin is a delta */
extern int isexplicitsmash;     /* true if \smash explicitly given */
extern int smashcheck;          /* check if terms safe to smash */
extern int isscripted;          /* is (lefthand) term text-scripted */
extern int isdelimscript;       /* is \right delim text-scripted */
extern int issmashokay;         /*is leading char okay for smashing */
extern int blanksignal;         /*rastsmash signal right-hand blank */
extern int blanksymspace;       /* extra (or too much) space wanted */
extern int aaalgorithm;         /* for lp, 1=aalowpass, 2 =aapnm */
extern int maxfollow;           /* aafollowline() maxturn parameter */
extern int fgalias;
extern int fgonly;
extern int bgalias;
extern int bgonly;              /* aapnm() params */
extern int issupersampling;     /*1=supersampling 0=lowpass */
extern int isss;                /* supersampling flag for main() */
extern int *workingparam;       /* working parameter */
extern subraster_t *workingbox;   /*working subraster_t box */
extern int isreplaceleft;       /* true to replace leftexpression */
extern subraster_t *leftexpression;/*rasterized so far */
extern const mathchardef_t *leftsymdef; /* mathchardef for preceding symbol */

extern int nfontinfo;           /* legal font#'s are 1...nfontinfo */
extern fontfamily_t *fonttable;

/* --- variables for anti-aliasing parameters --- */
extern int centerwt;            /*lowpass matrix center pixel wt */
extern int adjacentwt;          /*lowpass matrix adjacent pixel wt */
extern int cornerwt;            /*lowpass matrix corner pixel wt */
extern int minadjacent;         /* darken if>=adjacent pts black */
extern int maxadjacent;         /* darken if<=adjacent pts black */
extern int weightnum;           /* font wt, */
extern int maxaaparams;         /* #entries in table */

extern int fraccenterline;      /* baseline for punct. after \frac */

//------------------------------------------------------------------------------
// ctor / dtor
raster_t *new_raster(int width, int height, int pixsz);
subraster_t *new_subraster(int width, int height, int pixsz);
chardef_t *new_chardef(void);
void delete_raster(raster_t *rp);
void delete_subraster(subraster_t *sp);
void delete_chardef(chardef_t *cp);

// raster operations
raster_t *rastcpy(const raster_t * rp);
raster_t *rastref(const raster_t *rp, int axis);
raster_t *border_raster(raster_t * rp, int ntop, int nbot, int isline, int isfree);
raster_t *rastrot(const raster_t *rp);
raster_t *rastmag(const raster_t *rp, int a_magstep);

subraster_t *subrastcpy(const subraster_t *sp);

subraster_t *rastcompose(subraster_t *sp1, subraster_t *sp2, int offset2, int isalign, int isfree);
subraster_t *rastcat(subraster_t *sp1, subraster_t *sp2, int isfree);
subraster_t *rastack(subraster_t *sp1, subraster_t *sp2, int base, int space, int iscenter, int isfree);
subraster_t *arrow_subraster(int width, int height, int pixsz, int drctn, int isBig);
subraster_t *uparrow_subraster(int width, int height, int pixsz, int drctn, int isBig);
subraster_t *accent_subraster(int accent, int width, int height, int direction, int pixsz);
subraster_t *make_delim(const char *symbol, int height);
subraster_t *get_delim(const char *symbol, int height, int family);
subraster_t *rasterize(const char *expression, int size);
subraster_t *rastlimits(const char **expression, int size, subraster_t *basesp);
subraster_t *rastparen(const char **subexpr, int size, subraster_t * basesp);
subraster_t *rastscripts(const char **expression, int size, subraster_t * basesp);

int circle_raster(raster_t * rp, int row0, int col0, int row1, int col1, int thickness, const char *quads);
int circle_recurse(raster_t * rp, int row0, int col0, int row1, int col1, int thickness, double theta0, double theta1);
int rule_raster(raster_t *rp, int top, int left, int width, int height, int type);
raster_t *backspace_raster(raster_t * rp, int nback, int *pback, int minspace, int isfree);
int line_raster(raster_t *rp, int row0, int col0, int row1, int col1, int thickness);
int bezier_raster(raster_t *rp, double r0, double c0, double r1, double c1, double rt, double ct);

pixbyte_t *bytemapmag(const pixbyte_t *bytemap, int width, int height, int a_magstep);
int rastput(raster_t *target, const raster_t *source, int top, int left, int isopaque);
int evalterm(const store_t *store, const char *term);

// aa
int aasupsamp(const raster_t *rp, raster_t **aa, int sf, int grayscale);

// tex
char *texsubexpr(const char *expression, char *subexpr, int maxsubsz,
                 const char *left, const char *right, int isescape,
                 int isdelim);
const char *texscripts(const char *expression, char *subscript, char *superscript, int which);
char *texchar(const char *expression, char *chartoken);

// str
char *strtexchr(const char *string, const char *texchr);
char *strpspn(const char *s, const char *reject, char *segment);
char *strpspn(const char *s, const char *reject, char *segment);
char *strchange(int nfirst, char *from, const char *to);
char *strwstr(const char *string, const char *substr, const char *white, int *sublen);
int strreplace(char *string, const char *from, const char *to, int nreplace);
int isstrstr(const char *string, const char *snippets, int iscase);
int isnumeric(const char *s);
char *preamble(char *expression, int *size, char *subexpr);

//------------------------------------------------------------------------------
// Log functions

/* --- debugging and error reporting --- */
#define DBGLEVEL 9              /* debugging if msglevel>=DBGLEVEL */
#define LOGLEVEL 3              /* logging if msglevel>=LOGLEVEL */
#ifndef FORMLEVEL
#define FORMLEVEL LOGLEVEL      /*msglevel if called from html form */
#endif
#ifndef ERRORSTATUS             /* exit(ERRORSTATUS) for any error */
#define ERRORSTATUS 0           /* default doesn't signal errors */
#endif
/* --- embed warnings in rendered expressions, [\xxx?] if \xxx unknown --- */
#ifdef WARNINGS
#define WARNINGLEVEL WARNINGS
#else
#endif

#if DEBUG_LOG_LEVEL > 0
#define msgfp stderr
int type_raster(const raster_t *rp, FILE *fp);
int type_bytemap(const pixbyte_t *bp, int grayscale, int width, int height, FILE *fp);
#endif


#endif /* SMALLTEX_PRI_H */
