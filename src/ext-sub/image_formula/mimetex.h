#ifndef _MIMETEX
#define _MIMETEX
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

/* -------------------------------------------------------------------------
Program id
-------------------------------------------------------------------------- */
#define VERSION "1.74"          /* mimeTeX version number */
#define REVISIONDATE "26 Sept 2012" /* date of most recent revision */
#define COPYRIGHTTEXT "Copyright(c) 2002-2012, John Forkosh Associates, Inc"


/* --------------------------------------------------------------------------
check for compilation by parts (not supported yet)
-------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
miscellaneous macros
-------------------------------------------------------------------------- */
#define max2(x,y)  ((x)>(y)? (x):(y))   /* larger of 2 arguments */
#define min2(x,y)  ((x)<(y)? (x):(y))   /* smaller of 2 arguments */
#define max3(x,y,z) max2(max2(x,y),(z)) /* largest of 3 arguments */
#define min3(x,y,z) min2(min2(x,y),(z)) /* smallest of 3 arguments */
#define absval(x)  ((x)>=0?(x):(-(x)))  /* absolute value */
#define iround(x)  ((int)((x)>=0?(x)+0.5:(x)-0.5)) /* round double to int */
#define dmod(x,y)  ((x)-((y)*((double)((int)((x)/(y)))))) /*x%y for doubles*/

/* --------------------------------------------------------------------------
macros to get/set/unset a single bit (in rasters), and some bitfield macros
-------------------------------------------------------------------------- */
/* --- single-bit operations on a scalar argument (x) --- */
#define get1bit(x,bit)   ( ((x)>>(bit)) & 1 )   /* get the bit-th bit of x */
#define set1bit(x,bit)   ( (x) |=  (1<<(bit)) ) /* set the bit-th bit of x */
#define unset1bit(x,bit) ( (x) &= ~(1<<(bit)) ) /*unset the bit-th bit of x*/
/* --- single-bit operations on a byte-addressable argument (x) --- */
#define getlongbit(x,bit) get1bit(*((x)+(bit)/8),(bit)%8)   /* get bit */
#define setlongbit(x,bit) set1bit(*((x)+(bit)/8),(bit)%8)   /* set bit */
#define unsetlongbit(x,bit) unset1bit(*((x)+(bit)/8),(bit)%8)   /*unset bit*/
/* --- a few bitfield macros --- */
#define bitmask(nbits)  ((1<<(nbits))-1)    /* a mask of nbits 1's */
#define getbitfld(x,bit1,nbits) (((x)>>(bit1)) & (bitmask(nbits)))

/* --------------------------------------------------------------------------
macros to get/clear/set a single 4-bit nibble (in rasters)
-------------------------------------------------------------------------- */
#define getnibble(x,i)              /* get i-th 4-bit nibble */ \
    ( (i)%2==0? ((x)[(i)/2] & 0xF0) >> 4:   /* left/high nibble */      \
    (x)[(i)/2] & 0x0F )         /* right/low-order nibble */
#define clearnibble(x,i) ((x)[(i)/2] &= ((i)%2==0?0x0F:0xF0)) /*clear ith*/
#define setnibble(x,i,n)            /*set ith nibble of x to n*/\
    if ( (i)%2 == 0 )           /* setting left nibble */   \
      { clearnibble(x,i);           /* first clear the nibble*/ \
        (x)[(i)/2] |= ((n)&0x0F)<<4; }  /* set high-order 4 bits */ \
    else                    /* setting right nibble */  \
     if ( 1 )               /* dummy -- always true */  \
      { clearnibble(x,i);           /* first clear the nibble*/ \
        (x)[(i)/2] |= (n)&0x0F; }       /* set low-order 4 bits */  \
     else                   /* let user supply final ;*/
/* --- macros to get/set/clear byte (format=2) or nibble (format=3) --- */
#define getbyfmt(fmt,x,i)           /*byte(fmt=2) or nibble(3)*/\
    ( ((fmt)==2? ((int)((x)[(i)])) :    /* get full 8-bit byte */   \
       ((fmt)==3? getnibble(x,i) : 0)) )    /* or 4-bit nibble (err=0)*/
#define clearbyfmt(fmt,x,i)         /*byte(fmt=2) or nibble(3)*/\
    if((fmt)==2) (x)[(i)] = ((unsigned char)0); /* clear 8-bit byte */  \
    else if((fmt)==3) clearnibble(x,i)  /* or clear 4-bit nibble */
#define setbyfmt(fmt,x,i,n)         /*byte(fmt=2) or nibble(3)*/\
    if((fmt)==2) (x)[(i)] = ((unsigned char)n); /*set full 8-bit byte*/ \
    else if((fmt)==3) setnibble(x,i,n); else /* or set 4-bit nibble */

/* ---
 * associated raster constants and macros
 * -------------------------------------- */
#define maxraster 1048576 /*99999*/ /* max #pixels for raster pixmap */
/* --- #bytes in pixmap raster needed to contain width x height pixels --- */
#define bitmapsz(width,height) (((width)*(height)+7)/8) /*#bytes if a bitmap*/
#define pixmapsz(rp) (((rp)->pixsz)*bitmapsz((rp)->width,(rp)->height))
/* --- #bytes in raster struct, by its format --- */
#define pixbytes(rp) ((rp)->format==1? pixmapsz(rp) : /*#bytes in bitmap*/  \
    ((rp)->format==2? (rp)->pixsz : (1+(rp)->pixsz)/2) ) /*gf-formatted*/
/* --- pixel index calculation used by getpixel() and setpixel() below --- */
#define PIXDEX(rp,irow,icol) (((irow)*((rp)->width))+(icol))/*irow,icol indx*/
/* --- get value of pixel, either one bit or one byte, at (irow,icol) --- */
#define getpixel(rp,irow,icol)      /*get bit or byte based on pixsz*/  \
    ((rp)->pixsz==1? getlongbit((rp)->pixmap,PIXDEX(rp,(irow),(icol))) :\
     ((rp)->pixsz==8? ((rp)->pixmap)[PIXDEX(rp,(irow),(icol))] : (-1)) )
/* --- set value of pixel, either one bit or one byte, at (irow,icol) --- */
#define setpixel(rp,irow,icol,value)    /*set bit or byte based on pixsz*/  \
    if ( (rp)->pixsz == 1 )     /*set pixel to 1 or 0 for bitmap*/  \
     if ( (value) != 0 )        /* turn bit pixel on */             \
      { setlongbit((rp)->pixmap,PIXDEX(rp,(irow),(icol))); }            \
     else               /* or turn bit pixel 0ff */         \
      { unsetlongbit((rp)->pixmap,PIXDEX(rp,(irow),(icol))); }      \
    else                /* set 8-bit bytemap pixel value */ \
      if ( (rp)->pixsz == 8 )   /* check pixsz=8 for bytemap */     \
         ((rp)->pixmap)[PIXDEX(rp,(irow),(icol))]=(pixbyte_t)(value);     \
      else              /* let user supply final ; */

/* --------------------------------------------------------------------------
some char classes tokenizer needs to recognize, and macros to check for them
-------------------------------------------------------------------------- */
/* --- some character classes --- */
#define istextmode  (fontinfo[fontnum].istext==1) /* true for text font*/
#define WHITEMATH   "~ \t\n\r\f\v"  /* white chars in display/math mode*/
#define WHITETEXT   "\t\n\r\f\v"    /* white chars in text mode */
#define WHITEDELIM  "~ "        /*always ignored following \sequence*/
#define WHITESPACE  (istextmode?WHITETEXT:WHITEMATH) /*whitespace chars*/
#define LEFTBRACES  "{([<|-="   /* opening delims are left{([< |,|| */
#define RIGHTBRACES "})]>|-="   /* corresponding closing delims */
#define ESCAPE      "\\"        /* introduce escape sequence */
#define SUPERSCRIPT "^"     /* introduce superscript */
#define SUBSCRIPT   "_"     /* introduce subscript */
#define SCRIPTS     SUPERSCRIPT SUBSCRIPT /* either "script" */
/* --- macros to check for them --- */
#define isthischar(thischar,accept) \
    ( (thischar)!='\000' && *(accept)!='\000' \
    && strchr(accept,(thischar))!=(char *)NULL )
#define isthisstr(thisstr,accept) \
    ((*(thisstr))!='\000' && strspn(thisstr,accept)==strlen(thisstr))
#define skipwhite(thisstr)  if ( (thisstr) != NULL ) \
    while ( isthischar(*(thisstr),WHITESPACE) ) (thisstr)++
#define isnextchar(thisstr,accept) \
    ({skipwhite(thisstr);},isthischar(*thisstr,accept))


/* ---
aspect ratio is width/height of the displayed image of a pixel
-------------------------------------------------------------- */
#define ASPECTRATIO 1.0 /*(16.0/9.0)*/
#define SURDSERIFWIDTH(sqrtht) max2(1, ( 1 + (((sqrtht)+8)/20) ) )
#define SURDWIDTH(sqrtht,x) ( SURDSERIFWIDTH((sqrtht)) + \
        (((sqrtht)+1)*ASPECTRATIO + 1) / ((((sqrtht))/20)+(x))  )
        /* ((int)(.5*((double)((sqrtht)+1))*ASPECTRATIO + 0.5)) ) */
#define SQRTWIDTH(sqrtht,x) min2(32,max2(10,SURDWIDTH((sqrtht),(x))))


/* --- subraster types --- */
#define CHARASTER   (1)     /* character */
#define STRINGRASTER    (2)     /* string of characters */
#define IMAGERASTER (3)     /* image */
#define FRACRASTER  (4)     /* image of \frac{}{} */
#define ASCIISTRING (5)     /* ascii string (not a raster) */

/* ---
 * issue rasterize() call end extract embedded raster from returned subraster
 * -------------------------------------------------------------------------- */
#define make_raster(expression,size)    ((rasterize(expression,size))->image)



#endif

