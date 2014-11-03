/****************************************************************************
 *
 * Copyright(c) 2002-2012, John Forkosh Associates, Inc. All rights reserved.
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
 * Purpose:   o MimeTeX, licensed under the gpl, lets you easily embed
 *      LaTeX math in your html pages.  It parses a LaTeX math
 *      expression and immediately emits the corresponding gif
 *      image, rather than the usual TeX dvi.  And mimeTeX is an
 *      entirely separate little program that doesn't use TeX or
 *      its fonts in any way.  It's just one cgi that you put in
 *      your site's cgi-bin/ directory, with no other dependencies.
 *           So mimeTeX is very easy to install.  And it's equally
 *      easy to use.  Just place an html <img> tag in your document
 *      wherever you want to see the corresponding LaTeX expression.
 *      For example,
 *       <img src="../cgi-bin/mimetex.cgi?\int_{-\infty}^xe^{-t^2}dt"
 *        alt="" border=0 align=middle>
 *      immediately generates the corresponding gif image on-the-fly,
 *      displaying the rendered expression wherever you put that
 *      <img> tag.
 *           MimeTeX doesn't need intermediate dvi-to-gif conversion,
 *      and it doesn't clutter up your filesystem with separate
 *      little gif files for each converted expression.
 *      But image caching is available by using mimeTeX's
 *      -DCACHEPATH=\"path/\" compile option (see below).
 *      There's also no inherent need to repeatedly write the
 *      cumbersome <img> tag illustrated above.  You can write
 *      your own custom tags, or write a wrapper script around
 *      mimeTeX to simplify the notation.
 *           Further discussion about mimeTeX's features and
 *      usage is available on its homepage,
 *        http://www.forkosh.com/mimetex.html
 *      and similarly in mimetex.html included with your mimetex.zip
 *      distribution file. (Note: http://www.forkosh.com/mimetex.html
 *      is a "quickstart" version of the the full mimetex.html manual
 *      included in your mimetex.zip distribution file.)
 *
 * Functions:   The following "table of contents" lists each function
 *      comprising mimeTeX in the order it appears in this file.
 *      See individual function entry points for specific comments
 *      about its purpose, calling sequence, side effects, etc.
 *      (All these functions eventually belong in several
 *      different modules, possibly along the lines suggested
 *      by the divisions below.  But until the best decomposition
 *      becomes clear, it seems better to keep mimetex.c
 *      neatly together, avoiding a bad decomposition that
 *      becomes permanent by default.)
 *      ===================== Raster Functions ======================
 *  PART2   --- raster constructor functions ---
 *      new_raster(width,height,pixsz)   allocation (and constructor)
 *      new_subraster(width,height,pixsz)allocation (and constructor)
 *      new_chardef()                         allocate chardef struct
 *      delete_raster(rp)        deallocate raster (rp =  raster ptr)
 *      delete_subraster(sp)  deallocate subraster (sp=subraster ptr)
 *      delete_chardef(cp)      deallocate chardef (cp = chardef ptr)
 *      --- primitive (sub)raster functions ---
 *      rastcpy(rp)                           allocate new copy of rp
 *      subrastcpy(sp)                        allocate new copy of sp
 *      rastrot(rp)         new raster rotated right 90 degrees to rp
 *      rastmag(rp,magstep)   new raster magnified by "magstep" to rp
 *      bytemapmag(bytemap,width,height,magstep)      magnify bytemap
 *      rastref(rp,axis)    new raster reflected (axis 1=horz,2=vert)
 *      rastput(target,source,top,left,isopaque)  overlay src on trgt
 *      rastcompose(sp1,sp2,offset2,isalign,isfree) sp2 on top of sp1
 *      rastcat(sp1,sp2,isfree)                  concatanate sp1||sp2
 *      rastack(sp1,sp2,base,space,iscenter,isfree)stack sp2 atop sp1
 *      rastile(tiles,ntiles)      create composite raster from tiles
 *      rastsmash(sp1,sp2,xmin,ymin)      calc #smash pixels sp1||sp2
 *      rastsmashcheck(term)         check if term is "safe" to smash
 *      --- raster "drawing" functions ---
 *      accent_subraster(accent,width,height,direction,pixsz)\hat\vec
 *      arrow_subraster(width,height,drctn,isBig)    left/right arrow
 *      uparrow_subraster(width,height,drctn,isBig)     up/down arrow
 *      rule_raster(rp,top,left,width,height,type)    draw rule in rp
 *      line_raster(rp,row0,col0,row1,col1,thickness) draw line in rp
 *      line_recurse(rp,row0,col0,row1,col1,thickness)   recurse line
 *      circle_raster(rp,row0,col0,row1,col1,thickness,quads) ellipse
 *      circle_recurse(rp,row0,col0,row1,col1,thickness,theta0,theta1)
 *      bezier_raster(rp,r0,c0,r1,c1,rt,ct)   draw bezier recursively
 *      border_raster(rp,ntop,nbot,isline,isfree)put border around rp
 *      backspace_raster(rp,nback,pback,minspace,isfree)    neg space
 *      --- raster (and chardef) output functions ---
 *      type_raster(rp,fp)       emit ascii dump of rp on file ptr fp
 *      type_bytemap(bp,grayscale,width,height,fp) dump bytemap on fp
 *      xbitmap_raster(rp,fp)           emit mime xbitmap of rp on fp
 *      type_pbmpgm(rp,ptype,file)     pbm or pgm image of rp to file
 *      cstruct_chardef(cp,fp,col1)         emit C struct of cp on fp
 *      cstruct_raster(rp,fp,col1)          emit C struct of rp on fp
 *      hex_bitmap(rp,fp,col1,isstr)emit hex dump of rp->pixmap on fp
 *      --- ancillary output functions ---
 *      emit_string(fp,col1,string,comment) emit string and C comment
 *      gftobitmap(rp)        convert .gf-like pixmap to bitmap image
 *      ====================== Font Functions =======================
 *      --- font lookup functions ---
 *      get_symdef(symbol)              return mathchardef for symbol
 *      get_ligature(expr,family)  return symtable index for ligature
 *      get_chardef(symdef,size)       return chardef for symdef,size
 *      get_charsubraster(symdef,size)  wrap subraster around chardef
 *      get_symsubraster(symbol,size)    returns subraster for symbol
 *      --- ancillary font functions ---
 *      get_baseline(gfdata)       determine baseline (in our coords)
 *      get_delim(symbol,height,family) delim just larger than height
 *      make_delim(symbol,height) construct delim exactly height size
 *      ================= Tokenize/Parse Functions ==================
 *      texchar(expression,chartoken)  retruns next char or \sequence
 *      texsubexpr(expr,subexpr,maxsubsz,left,right,isescape,isdelim)
 *      texleft(expr,subexpr,maxsubsz,ldelim,rdelim)   \left...\right
 *      texscripts(expression,subscript,superscript,which)get scripts
 *      --- ancillary parse functions ---
 *      isbrace(expression,braces,isescape)   check for leading brace
 *      preamble(expression,size,subexpr)              parse preamble
 *      mimeprep(expression) preprocessor converts \left( to \(, etc.
 *      strchange(nfirst,from,to)   change nfirst chars of from to to
 *      strreplace(string,from,to,nreplace)  change from to to in str
 *      strwstr(string,substr,white,sublen)     find substr in string
 *      strdetex(s,mode)    replace math chars like \^_{} for display
 *      strtexchr(string,texchr)                find texchr in string
 *      findbraces(expression,command)    find opening { or closing }
 *      strpspn(s,reject,segment)     non-() chars of s not in reject
 *      isstrstr(string,snippets,iscase)  are any snippets in string?
 *      isnumeric(s)                     determine if s is an integer
 *      evalterm(store,term)     evaluate numeric value of expression
 *      getstore(store,identifier)return value corresponding to ident
 *      unescape_url(url,isescape), x2c(what)   xlate %xx url-encoded
 *  PART3   =========== Rasterize an Expression (recursively) ===========
 *      --- here's the primary entry point for all of mimeTeX ---
 *      rasterize(expression,size)     parse and rasterize expression
 *      --- explicitly called handlers that rasterize... ---
 *      rastparen(subexpr,size,basesp)          parenthesized subexpr
 *      rastlimits(expression,size,basesp)    dispatch super/sub call
 *      rastscripts(expression,size,basesp) super/subscripted exprssn
 *      rastdispmath(expression,size,sp)      scripts for displaymath
 *      --- table-driven handlers that rasterize... ---
 *      rastleft(expression,size,basesp,ildelim,arg2,arg3)\left\right
 *      rastright(expression,size,basesp,ildelim,arg2,arg3) ...\right
 *      rastmiddle(expression,size,basesp,arg1,arg2,arg3)     \middle
 *      rastflags(expression,size,basesp,flag,value,arg3)    set flag
 *      rastspace(expression,size,basesp,width,isfill,isheight)\,\:\;
 *      rastnewline(expression,size,basesp,arg1,arg2,arg3)         \\
 *      rastarrow(expression,size,basesp,width,height,drctn) \longarr
 *      rastuparrow(expression,size,basesp,width,height,drctn)up/down
 *      rastoverlay(expression,size,basesp,overlay,arg2,arg3)    \not
 *      rastfrac(expression,size,basesp,isfrac,arg2,arg3) \frac \atop
 *      rastackrel(expression,size,basesp,base,arg2,arg3)   \stackrel
 *      rastmathfunc(expression,size,basesp,base,arg2,arg3) \lim,\etc
 *      rastsqrt(expression,size,basesp,arg1,arg2,arg3)         \sqrt
 *      rastaccent(expression,size,basesp,accent,isabove,isscript)
 *      rastfont(expression,size,basesp,font,arg2,arg3) \cal{},\scr{}
 *      rastbegin(expression,size,basesp,arg1,arg2,arg3)     \begin{}
 *      rastarray(expression,size,basesp,arg1,arg2,arg3)       \array
 *      rastpicture(expression,size,basesp,arg1,arg2,arg3)   \picture
 *      rastline(expression,size,basesp,arg1,arg2,arg3)         \line
 *      rastrule(expression,size,basesp,arg1,arg2,arg3)         \rule
 *      rastcircle(expression,size,basesp,arg1,arg2,arg3)     \circle
 *      rastbezier(expression,size,basesp,arg1,arg2,arg3)     \bezier
 *      rastraise(expression,size,basesp,arg1,arg2,arg3)    \raisebox
 *      rastrotate(expression,size,basesp,arg1,arg2,arg3)  \rotatebox
 *      rastmagnify(expression,size,basesp,arg1,arg2,arg3)   \magnify
 *      rastreflect(expression,size,basesp,arg1,arg2,arg3)\reflectbox
 *      rastfbox(expression,size,basesp,arg1,arg2,arg3)         \fbox
 *      rastinput(expression,size,basesp,arg1,arg2,arg3)       \input
 *      rastcounter(expression,size,basesp,arg1,arg2,arg3)   \counter
 *      rasteval(expression,size,basesp,arg1,arg2,arg3)         \eval
 *      rasttoday(expression,size,basesp,arg1,arg2,arg3)       \today
 *      rastcalendar(expression,size,basesp,arg1,arg2,arg3) \calendar
 *      rastenviron(expression,size,basesp,arg1,arg2,arg3)   \environ
 *      rastmessage(expression,size,basesp,arg1,arg2,arg3)   \message
 *      rastnoop(expression,size,basesp,arg1,arg2,arg3) flush \escape
 *      --- helper functions for handlers ---
 *      rastopenfile(filename,mode)      opens filename[.tex] in mode
 *      rasteditfilename(filename)       edit filename (for security)
 *      rastreadfile(filename,islock,tag,value)   read <tag>...</tag>
 *      rastwritefile(filename,tag,value,isstrict)write<tag>...</tag>
 *      calendar(year,month,day)    formats one-month calendar string
 *      timestamp(tzdelta,ifmt)              formats timestamp string
 *      tzadjust(tzdelta,year,month,day,hour)        adjust date/time
 *      daynumber(year,month,day)     #days since Monday, Jan 1, 1973
 *      strwrap(s,linelen,tablen)insert \n's and spaces to wrap lines
 *      strnlower(s,n)        lowercase the first n chars of string s
 *      urlprune(url,n)  http://abc.def.ghi.com/etc-->abc.def.ghi.com
 *      urlncmp(url1,url2,n)   compares topmost n levels of two url's
 *      dbltoa(d,npts)                double to comma-separated ascii
 *      === Anti-alias completed raster (lowpass) or symbols (ss) ===
 *      aalowpass(rp,bytemap,grayscale)     lowpass grayscale bytemap
 *      aapnm(rp,bytemap,grayscale)       lowpass based on pnmalias.c
 *      aapnmlookup(rp,bytemap,grayscale)  aapnm based on aagridnum()
 *      aapatterns(rp,irow,icol,gridnum,patternum,grayscale) call 19,
 *      aapattern1124(rp,irow,icol,gridnum,grayscale)antialias pattrn
 *      aapattern19(rp,irow,icol,gridnum,grayscale) antialias pattern
 *      aapattern20(rp,irow,icol,gridnum,grayscale) antialias pattern
 *      aapattern39(rp,irow,icol,gridnum,grayscale) antialias pattern
 *      aafollowline(rp,irow,icol,direction)       looks for a "turn"
 *      aagridnum(rp,irow,icol)             calculates gridnum, 0-511
 *      aapatternnum(gridnum)    looks up pattern#, 1-51, for gridnum
 *      aalookup(gridnum)     table lookup for all possible 3x3 grids
 *      aalowpasslookup(rp,bytemap,grayscale)   driver for aalookup()
 *      aasupsamp(rp,aa,sf,grayscale)             or by supersampling
 *      aacolormap(bytemap,nbytes,colors,colormap)make colors,colormap
 *      aaweights(width,height)      builds "canonical" weight matrix
 *      aawtpixel(image,ipixel,weights,rotate) weight image at ipixel
 *      === miscellaneous ===
 *      mimetexsetmsg(newmsglevel,newmsgfp)    set msglevel and msgfp
 *  PART1   ========================== Driver ===========================
 *      main(argc,argv) parses math expression and emits mime xbitmap
 *      CreateGifFromEq(expression,gifFileName)  entry pt for win dll
 *      ismonth(month)          is month current month ("jan"-"dec")?
 *      logger(fp,msglevel,logvars)        logs environment variables
 *      emitcache(cachefile,maxage,valign,isbuffer)    emit cachefile
 *      readcachefile(cachefile,buffer)    read cachefile into buffer
 *      advertisement(expression,mode)  wrap expression in ad message
 *      crc16(s)                               16-bit crc of string s
 *      md5str(instr)                      md5 hash library functions
 *      GetPixel(x,y)           callback function for gifsave library
 *
 * Source:  mimetex.c  (needs mimetex.h and texfonts.h to compile,
 *      and also needs gifsave.c when compiled with -DAA or -DGIF)
 *
 * --------------------------------------------------------------------------
 * Notes      o See individual function entry points for specific comments
 *      about the purpose, calling sequence, side effects, etc
 *      of each mimeTeX function listed above.
 *        o See bottom of file for main() driver (and "friends"),
 *      and compile as
 *         cc -DAA mimetex.c gifsave.c -lm -o mimetex.cgi
 *      to produce an executable that emits gif images with
 *      anti-aliasing (see Notes below).  You may also compile
 *         cc -DGIF mimetex.c gifsave.c -lm -o mimetex.cgi
 *      to produce an executable that emits gif images without
 *      anti-aliasing.  Alternatively, compile mimeTeX as
 *         cc -DXBITMAP mimetex.c -lm -o mimetex.cgi
 *      to produce an executable that just emits mime xbitmaps.
 *      In either case you'll need mimetex.h and texfonts.h,
 *      and with -DAA or -DGIF you'll also need gifsave.c
 *        o The font information in texfonts.h was produced by multiple
 *      runs of gfuntype, one run per struct (i.e., one run per font
 *      family at a particular size).  Compile gfuntype as
 *         cc gfuntype.c mimetex.c -lm -o gfuntype
 *      See gfuntype.c, and also mimetex.html#fonts, for details.
 *        o For gif images, the gifsave.c library by Sverre H. Huseby
 *      <http://shh.thathost.com> slightly modified by me to allow
 *      (a)sending output to stdout or returning it in memory,
 *      and (b)specifying a transparent background color index,
 *      is included with mimeTeX, and it's documented in
 *      mimetex.html#gifsave
 *        o MimeTeX's principal reusable function is rasterize(),
 *      which takes a string like "f(x)=\int_{-\infty}^xe^{-t^2}dt"
 *      and returns a (sub)raster representing it as a bit or bytemap.
 *      Your application can do anything it likes with this pixel map.
 *      MimeTeX just outputs it, either as a mime xbitmap or as a gif.
 *      See  mimetex.html#makeraster  for further discussion
 *      and examples.
 *        o File mimetex.c also contains library functions implementing
 *      a raster datatype, functions to manipulate rasterized .mf
 *      fonts (see gfuntype.c which rasterizes .mf fonts), functions
 *      to parse LaTeX expressions, etc.  As already mentioned,
 *      a complete list of mimetex.c functions is above.  See their
 *      individual entry points below for further comments.
 *         As also mentioned, these functions eventually belong in
 *      several different modules, possibly along the lines suggested
 *      by the divisions above.  But until the best decomposition
 *      becomes clear, it seems better to keep mimetex.c
 *      neatly together, avoiding a bad decomposition that
 *      becomes permanent by default.
 *        o Optional compile-line -D defined symbols are documented
 *      in mimetex.html#options .  They include (additional -D
 *      switches are discussed at mimetex.html#options)...
 *      -DAA
 *          Turns on gif anti-aliasing with default values
 *          (CENTERWT=32, ADJACENTWT=3, CORNERWT=1)
 *          for the following anti-aliasing parameters...
 *      -DCENTERWT=n
 *      -DADJACENTWT=j
 *      -DCORNERWT=k
 *          *** Note: Ignore these three switches because
 *          *** mimeTeX's current anti-aliasing algorithm
 *          *** no longer uses them (as of version 1.60).
 *          MimeTeX currently provides a lowpass filtering
 *          algorithm for anti-aliasing, which is applied to the
 *          existing set of bitmap fonts.  This lowpass filter
 *          applies default weights
 *              1   2   1
 *              2   8   2
 *              1   2   1
 *          to neighboring pixels. The defaults weights are
 *          CENTERWT=8, ADJACENTWT=2 and CORNERWT=1,
 *          which you can adjust to control anti-aliasing.
 *          Lower CENTERWT values will blur/spread out lines
 *          while higher values will tend to sharpen lines.
 *          Experimentation is recommended to determine
 *          what value works best for you.
 *      -DCACHEPATH=\"path/\"
 *          This option saves each rendered image to a file
 *          in directory  path/  which mimeTeX reads rather than
 *          re-rendering the same image every time it's given
 *          the same LaTeX expression.  Sometimes mimeTeX disables
 *          caching, e.g., expressions containing \input{ } are
 *          re-rendered since the contents of the inputted file
 *          may have changed.  If compiled without -DCACHEPATH
 *          mimeTeX always re-renders expressions.  This usually
 *          isn't too cpu intensive, but if you have unusually
 *          high hit rates then image caching may be helpful.
 *          The  path/  is relative to mimetex.cgi, and must
 *          be writable by it.  Files created under  path/  are
 *          named filename.gif, where filename is the 32-character
 *          MD5 hash of the LaTeX expression.
 *      -DDEFAULTSIZE=n
 *          MimeTeX currently has eight font sizes numbered 0-7,
 *          and always starts in DEFAULTSIZE whose default value
 *          is 3 (corresponding to \large). Specify -DDEFAULTSIZE=4
 *          on the compile line if you prefer mimeTeX to start in
 *          larger default size 4 (corresponding to \Large), etc.
 *      -DDISPLAYSIZE=n
 *          By default, operator limits like \int_a^b are rendered
 *          \textstyle at font sizes \normalsize and smaller,
 *          and rendered \displaystyle at font sizes \large and
 *          larger.  This default corresponds to -DDISPLAYSIZE=3,
 *          which you can adjust; e.g., -DDISPLAYSIZE=0 always
 *          defaults to \displaystyle, and 99 (or any large number)
 *          always defaults to \textstyle.  Note that explicit
 *          \textstyle, \displaystyle, \limits or \nolimits
 *          directives in an expression always override
 *          the DISPLAYSIZE default.
 *      -DERRORSTATUS=n
 *          The default, 0, means mimeTeX always exits with status 0,
 *          regardless of whether or not it detects error(s) while
 *          trying to render your expression.  Specify any non-zero
 *          value (typically -1) if you write a script/plugin for
 *          mimeTeX that traps non-zero exit statuses.  MimeTeX then
 *          exits with its own non-zero status when it detects an
 *          error it can identify, or with your ERRORSTATUS value
 *          for errors it can't specifically identify.
 *      -DREFERER=\"domain\"   -or-
 *      -DREFERER=\"domain1,domain2,etc\"
 *          Blocks mimeTeX requests from unauthorized domains that
 *          may be using your server's mimetex.cgi without permission.
 *          If REFERER is defined, mimeTeX checks for the environment
 *          variable HTTP_REFERER and, if it exists, performs a
 *          case-insensitive test to make sure it contains 'domain'
 *          as a substring.  If given several 'domain's (second form)
 *          then HTTP_REFERER must contain either 'domain1' or
 *          'domain2', etc, as a (case-insensitive) substring.
 *          If HTTP_REFERER fails to contain a substring matching
 *          any of these domain(s), mimeTeX emits an error message
 *          image corresponding to the expression specified by
 *          the  invalid_referer_msg  string defined in main().
 *          Note: if HTTP_REFERER is not an environment variable,
 *          mimeTeX correctly generates the requested expression
 *          (i.e., no referer error).
 *      -DWARNINGS=n  -or-
 *      -DNOWARNINGS
 *          If an expression submitted to mimeTeX contains an
 *          unrecognzied escape sequence, e.g., "y=x+\abc+1", then
 *          mimeTeX generates a gif image containing an embedded
 *          warning in the form "y=x+[\abc?]+1".  If you want these
 *          warnings suppressed, -DWARNINGS=0 or -DNOWARNINGS tells
 *          mimeTeX to ignore unrecognized symbols, and the rendered
 *          image is "y=x++1" instead.
 *      -DWHITE
 *          MimeTeX usually renders black symbols on a white
 *          background.  This option renders white symbols on
 *          a black background instead.
 * --------------------------------------------------------------------------
 * Revision History:
 * 09/18/02 J.Forkosh   Installation.
 * 12/11/02 J.Forkosh   Version 1.00 released.
 * 07/04/03 J.Forkosh   Version 1.01 released.
 * 10/17/03 J.Forkosh   Version 1.20 released.
 * 12/21/03 J.Forkosh   Version 1.30 released.
 * 02/01/04 J.Forkosh   Version 1.40 released.
 * 10/02/04 J.Forkosh   Version 1.50 released.
 * 11/30/04 J.Forkosh   Version 1.60 released.
 * 10/11/05 J.Forkosh   Version 1.64 released.
 * 11/30/06 J.Forkosh   Version 1.65 released.
 * 09/06/08 J.Forkosh   Version 1.70 released.
 * 03/23/09 J.Forkosh   Version 1.71 released.
 * 11/18/09 J.Forkosh   Version 1.72 released.
 * 11/15/11 J.Forkosh   Version 1.73 released.
 * 02/15/12 J.Forkosh   Version 1.74 released.
 * 09/26/12 J.Forkosh   Most recent revision (also see REVISIONDATE)
 * See  http://www.forkosh.com/mimetexchangelog.html  for further details.
 *
 ****************************************************************************/

#include "mimetex.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>


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



/* -------------------------------------------------------------------------
adjustable default values
-------------------------------------------------------------------------- */
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
#define ADJACENTWT 2            /* anti-aliasing adjacentwt default */
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

/* --- variables for anti-aliasing parameters --- */
static int centerwt = CENTERWT; /*lowpass matrix center pixel wt */
static int adjacentwt = ADJACENTWT;     /*lowpass matrix adjacent pixel wt */
static int cornerwt = CORNERWT; /*lowpass matrix corner pixel wt */
static int minadjacent = MINADJACENT;   /* darken if>=adjacent pts black */
static int maxadjacent = MAXADJACENT;   /* darken if<=adjacent pts black */
static int weightnum = 1;       /* font wt, */
static int maxaaparams = 4;     /* #entries in table */

/* --- anti-aliasing parameter values by font weight --- */
static aaparameters aaparams[]  /* set params by weight */
    = {                         /* ----------------------------------------------------
                                   centerwt adj corner minadj max  fgalias,only,bgalias,only
                                   ------------------------------------------------------- */
    {64, 1, 1, 6, 8, 1, 0, 0, 0},       /* 0 = light */
    {CENTERWT, ADJACENTWT, CORNERWT, MINADJACENT, MAXADJACENT, 1, 0, 0, 0},
    {8, 1, 1, 5, 8, 1, 0, 0, 0},        /* 2 = semibold */
    {8, 2, 1, 4, 9, 1, 0, 0, 0} /* 3 = bold */
};

/* --- anti-aliasing diagnostics (to help improve algorithm) --- */
static int patternnumcount0[99];
static int patternnumcount1[99];        /*aalookup() counts */
static int ispatternnumcount = 1;       /* true to accumulate counts */

/* -------------------------------------------------------------------------
other variables
-------------------------------------------------------------------------- */
/* --- black on white background (default), or white on black --- */
#ifdef WHITE
#define ISBLACKONWHITE 0        /* white on black background */
#else
#define ISBLACKONWHITE 1        /* black on white background */
#endif
/* --- advertisement
   one image in every ADFREQUENCY is wrapped in "advertisement" --- */
#if !defined(ADFREQUENCY)
#define ADFREQUENCY 0           /* never show advertisement if 0 */
#endif
#ifndef HOST_SHOWAD
#define HOST_SHOWAD "\0"      /* show ads on all hosts */
#endif
/* --- "smash" margin (0 means no smashing) --- */
#ifndef SMASHMARGIN
#ifdef NOSMASH
#define SMASHMARGIN 0
#else
#define SMASHMARGIN 3
#endif
#endif
#ifndef SMASHCHECK
#define SMASHCHECK 0
#endif
/* --- textwidth --- */
#ifndef TEXTWIDTH
#define TEXTWIDTH (400)
#endif
/* --- font "combinations" --- */
#define CMSYEX (109)            /*select CMSY10, CMEX10 or STMARY10 */

/* -------------------------------------------------------------------------
debugging and logging / error reporting
-------------------------------------------------------------------------- */
/* --- debugging and error reporting --- */
#ifndef MSGLEVEL
#define MSGLEVEL 1
#endif
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
#ifdef NOWARNINGS
#define WARNINGLEVEL 0
#else
#define WARNINGLEVEL 1
#endif
#endif

static int warninglevel = WARNINGLEVEL; /* warning level */
static int msglevel = MSGLEVEL; /* message level for verbose/debug */
static FILE *msgfp;             /* output in command-line mode */

/* -------------------------------------------------------------------------
control flags and values
-------------------------------------------------------------------------- */
static int daemonlevel = 0;     /* incremented in main() */
static int recurlevel = 0;      /* inc/decremented in rasterize() */
static int scriptlevel = 0;     /* inc/decremented in rastlimits() */
static int isstring = 0;        /*pixmap is ascii string, not raster */
static int isligature = 0;      /* true if ligature found */
static const char *subexprptr = NULL;   /* ptr within expression to subexpr */
/*SHARED(int,imageformat,1); *//* image is 1=bitmap, 2=.gf-like */
static int isdisplaystyle = 1;  /* displaystyle mode (forced if 2) */
static int ispreambledollars = 0;       /* displaystyle mode set by $$...$$ */
static int fontnum = 0;         /* cal=1,scr=2,rm=3,it=4,bb=5,bf=6 */
static int fontsize = NORMALSIZE;       /* current size */
static int magstep = 1;         /* magstep (1=no change) */
static int displaysize = DISPLAYSIZE;   /* use \displaystyle when fontsize>= */
static int shrinkfactor = 3;    /* shrinkfactors[fontsize] */
static int rastlift = 0;        /* rastraise() lift parameter */
static int rastlift1 = 0;       /* rastraise() lift for base exprssn */
static double unitlength = 1.0; /* #pixels per unit (may be <1.0) */
static int iunitlength = 1;     /* #pixels per unit as int for store */
/*static int,textwidth = TEXTWIDTH; *//* #pixels across line */
//static int, adfrequency, ADFREQUENCY;    /* advertisement frequency */
static int isnocatspace = 0;   /* >0 to not add space in rastcat() */
static int smashmargin = SMASHMARGIN;  /* minimum "smash" margin */
static int mathsmashmargin = SMASHMARGIN;      /* needed for \text{if $n-m$ even} */
static int issmashdelta = 1;   /* true if smashmargin is a delta */
static int isexplicitsmash = 0;        /* true if \smash explicitly given */
static int smashcheck = SMASHCHECK;    /* check if terms safe to smash */
//static int, isnomath = 0;     /* true to inhibit math mode */
static int isscripted = 0;     /* is (lefthand) term text-scripted */
static int isdelimscript = 0;  /* is \right delim text-scripted */
static int issmashokay = 0;    /*is leading char okay for smashing */
#define BLANKSIGNAL (-991234)   /*rastsmash signal right-hand blank */
static int blanksignal = BLANKSIGNAL;  /*rastsmash signal right-hand blank */
static int blanksymspace = 0;  /* extra (or too much) space wanted */
static int aaalgorithm = AAALGORITHM;  /* for lp, 1=aalowpass, 2 =aapnm */
static int maxfollow = MAXFOLLOW;      /* aafollowline() maxturn parameter */
static int fgalias = 1;
static int fgonly = 0;
static int bgalias = 0;
static int bgonly = 0;         /* aapnm() params */
static int issupersampling = ISSUPERSAMPLING;  /*1=supersampling 0=lowpass */
static int isss = ISSUPERSAMPLING;     /* supersampling flag for main() */
static int *workingparam = NULL;       /* working parameter */
static subraster *workingbox = NULL;   /*working subraster box */
static int isreplaceleft = 0;  /* true to replace leftexpression */
static subraster *leftexpression = NULL;       /*rasterized so far */
static mathchardef *leftsymdef = NULL; /* mathchardef for preceding symbol */
static int fraccenterline = NOVALUE;   /* baseline for punct. after \frac */
/*static int,currentcharclass = NOVALUE; *//*primarily to check for PUNCTION */

/* -------------------------------------------------------------------------
store for evalterm() [n.b., these are stripped-down funcs from nutshell]
-------------------------------------------------------------------------- */

#define MAXSTORE 100            /* max 100 identifiers */
static STORE mimestore[MAXSTORE] = {
    {"fontsize", &fontsize}, {"fs", &fontsize}, /* font size */
    {"fontnum", &fontnum}, {"fn", &fontnum},    /* font number */
    {"unitlength", &iunitlength},       /* unitlength */
    /*{ "mytestvar", &mytestvar }, */
    {NULL, NULL}
};

/* --- supersampling shrink factors corresponding to displayed sizes --- */
static int shrinkfactors[] =    /*supersampling shrinkfactor by size */
{ 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

/* ---
 * space between adjacent symbols, e.g., symspace[RELATION][VARIABLE]
 * ------------------------------------------------------------------ */
static int symspace[11][11] = { /* -----------------------------------------------------------------------
                                   Right... ORD OPER  BIN  REL OPEN CLOS PUNC  VAR DISP SPACE unused
                                   Left... -------------------------------------------------------------- */
     /*ORDINARY*/ {2, 3, 3, 5, 3, 2, 2, 2, 3, 0, 0},
     /*OPERATOR*/ {3, 1, 1, 5, 3, 2, 2, 2, 3, 0, 0},
     /*BINARYOP*/ {2, 1, 1, 5, 3, 2, 2, 2, 3, 0, 0},
     /*RELATION*/ {5, 5, 5, 2, 5, 5, 2, 5, 5, 0, 0},
     /*OPENING*/ {2, 2, 2, 5, 2, 4, 2, 2, 3, 0, 0},
     /*CLOSING*/ {2, 3, 3, 5, 4, 2, 1, 2, 3, 0, 0},
     /*PUNCTION*/ {2, 2, 2, 5, 2, 2, 1, 2, 2, 0, 0},
     /*VARIABLE*/ {2, 2, 2, 5, 2, 2, 1, 2, 2, 0, 0},
     /*DISPOPER*/ {2, 3, 3, 5, 2, 3, 2, 2, 2, 0, 0},
     /*SPACEOPER*/ {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    /*unused */ {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

/* --- select current font table (for lowpass or supersampling) --- */
static fontfamily *fonttable =
    (ISSUPERSAMPLING ? ssfonttable : aafonttable);


/* ---
 * font family information
 * ----------------------- */
static int nfontinfo = 11;      /* legal font#'s are 1...nfontinfo */

static struct {
    const char *name;
    int family;
    int istext;
    int class;
}
  /* note: class(1=upper,2=alpha,3=alnum,4=lower,5=digit,9=all) now unused */
    fontinfo[]
    = {                         /* --- name family istext class --- */
    {
    "\\math", 0, 0, 0},         /*(0) default math mode */
    {
    "\\mathcal", CMSY10, 0, 1}, /*(1) calligraphic, uppercase */
    {
    "\\mathscr", RSFS10, 0, 1}, /*(2) rsfs/script, uppercase */
    {
    "\\textrm", CMR10, 1, -1},  /*(3) \rm,\text{abc} --> {\textrm~abc} */
    {
    "\\textit", CMMI10, 1, -1}, /*(4) \it,\textit{abc}-->{\textit~abc} */
    {
    "\\mathbb", BBOLD10, 0, -1},        /*(5) \bb,\mathbb{abc}-->{\mathbb~abc} */
    {
    "\\mathbf", CMMIB10, 0, -1},        /*(6) \bf,\mathbf{abc}-->{\mathbf~abc} */
    {
    "\\mathrm", CMR10, 0, -1},  /*(7) \mathrm */
    {
    "\\cyr", CYR10, 1, -1},     /*(8) \cyr (defaults as text mode) */
    {
    "\\textgreek", CMMI10GR, 1, -1},    /*(9) \textgreek{ab}-->\alpha\beta */
    {
    "\\textbfgreek", CMMI10BGR, 1, -1}, /*(10)\textbfgreek{ab}-->\alpha\beta */
    {
    "\\textbbgreek", BBOLD10GR, 1, -1}, /*(11)\textbbgreek{ab}-->\alpha\beta */
    {
NULL, 0, 0, 0}};


/* --- sqrt --- */
#define SQRTACCENT  (1)         /* \sqrt */
/* --- accents --- */
#define BARACCENT   (11)        /* \bar \overline */
#define UNDERBARACCENT  (12)    /* \underline */
#define HATACCENT   (13)        /* \hat */
#define DOTACCENT   (14)        /* \dot */
#define DDOTACCENT  (15)        /* \ddot */
#define VECACCENT   (16)        /* \vec */
#define TILDEACCENT (17)        /* \tilde */
#define OVERBRACE   (18)        /* \overbrace */
#define UNDERBRACE  (19)        /* \underbrace */
/* --- flags/modes --- */
#define ISFONTFAM   (1)         /* set font family */
#define ISDISPLAYSTYLE  (2)     /* set isdisplaystyle */
#define ISDISPLAYSIZE   (21)    /* set displaysize */
#define ISFONTSIZE  (3)         /* set fontsize */
#define ISMAGSTEP   (31)        /* set magstep */
#define ISWEIGHT    (4)         /* set aa params */
//#define   ISOPAQUE    (5)     /* set background opaque */
#define ISSUPER     (6)         /* set supersampling/lowpass */
#define ISAAALGORITHM   (61)    /* set anti-aliasing algorithm */
#define ISCENTERWT  (62)        /* set anti-aliasing center weight */
#define ISADJACENTWT    (63)    /* set anti-aliasing adjacent weight */
#define ISCORNERWT  (64)        /* set anti-aliasing adjacent weight */
//#define   PNMPARAMS   (65)        /* set fgalias,fgonly,bgalias,bgonly*/
//#define   ISGAMMA     (66)        /* set gamma correction */
#define ISSHRINK    (7)         /* set supersampling shrinkfactor */
#define UNITLENGTH  (8)         /* set unitlength */
//#define   ISCOLOR     (9)     /* set color */
#define ISREVERSE   (10)        /* set reverse video colors */
#define ISSTRING    (11)        /* set ascii string mode */
#define ISSMASH     (12)        /* set (minimum) "smash" margin */
//#define   ISCONTENTTYPE   (13)        /*enable/disable Content-type lines*/
//#define   ISCONTENTCACHED (14)        /* write Content-type to cache file*/
//#define   ISPBMPGM    (15)        /* write pbm/pgm (instead of gif) */

/* ---
 * handler functions for math operations
 * ------------------------------------- */
static subraster *rastflags(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);      /* set flags, e.g., for \rm */
static subraster *rastfrac(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);       /* handle \frac \atop expressions */
static subraster *rastackrel(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);     /* handle \stackrel expressions */
static subraster *rastmathfunc(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);   /* handle \lim,\log,etc expressions */
static subraster *rastoverlay(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);    /* handle \not */
static subraster *rastspace(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);      /* handle math space, \hspace,\hfill */
static subraster *rastnewline(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);    /* handle \\ newline */
static subraster *rastarrow(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);      /* handle \longrightarrow, etc */
static subraster *rastuparrow(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);    /* handle \longuparrow, etc */
static subraster *rastsqrt(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);       /* handle \sqrt */
static subraster *rastaccent(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);     /* handle \hat \vec \braces, etc */
static subraster *rastfont(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);       /* handle \cal{} \scr{}, etc */
static subraster *rastbegin(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);      /* handle \begin{}...\end{} */
static subraster *rastleft(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);       /* handle \left...\right */
static subraster *rastmiddle(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);     /* handle \left...\middle...\right */
static subraster *rastarray(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);      /* handle \array{...} */
static subraster *rastpicture(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);    /* handle \picture(,){...} */
static subraster *rastline(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);       /* handle \line(xinc,yinc){xlen} */
static subraster *rastrule(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);       /* handle \rule[lift]{width}{height} */
static subraster *rastcircle(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);     /* handle \circle(xdiam[,ydiam]) */
static subraster *rastbezier(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);     /*handle\bezier(c0,r0)(c1,r1)(ct,rt) */
static subraster *rastraise(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);      /* handle \raisebox{lift}{expr} */
static subraster *rastrotate(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);     /* handle \rotatebox{degs}{expr} */
static subraster *rastmagnify(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);    /* handle \magnify{magstep}{expr} */
static subraster *rastreflect(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);    /* handle \reflectbox[axis]{expr} */
static subraster *rastfbox(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);       /* handle \fbox{expr} */
//static subraster *rastinput(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);            /* handle \input{filename} */
//static subraster *rastcounter(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);      /* handle \counter{filename} */
static subraster *rasteval(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);       /* handle \eval{expression} */
//static subraster *rasttoday(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);            /* handle \today[+/-tzdelta,ifmt] */
//static subraster *rastcalendar(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);     /* handle \calendar[yaer,month] */
//static subraster *rastenviron(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);      /* handle \environment */
//static subraster *rastmessage(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);      /* handle \message */
static subraster *rastnoop(const char **expression, int size, subraster * basesp, int flag, int value, int arg3);       /* handle \escape's to be flushed */

/* ---
 * mathchardefs for symbols recognized by mimetex
 * ---------------------------------------------- */
static mathchardef symtable[] = {
    /* ---------- c o m m a n d  h a n d l e r s --------------
       symbol    arg1     arg2     arg3       function
       -------------------------------------------------------- */
    /* --- commands --- */
    {"\\left", NOVALUE, NOVALUE, NOVALUE, rastleft},
    {"\\middle", NOVALUE, NOVALUE, NOVALUE, rastmiddle},
    {"\\frac", 1, NOVALUE, NOVALUE, rastfrac},
    {"\\over", 1, NOVALUE, NOVALUE, rastfrac},
    {"\\atop", 0, NOVALUE, NOVALUE, rastfrac},
    {"\\choose", 0, NOVALUE, NOVALUE, rastfrac},
    {"\\not", 1, 0, NOVALUE, rastoverlay},
    {"\\Not", 2, 0, NOVALUE, rastoverlay},
    {"\\widenot", 2, 0, NOVALUE, rastoverlay},
    {"\\sout", 3, NOVALUE, NOVALUE, rastoverlay},
    {"\\strikeout", 3, NOVALUE, NOVALUE, rastoverlay},
    {"\\compose", NOVALUE, NOVALUE, NOVALUE, rastoverlay},
    {"\\stackrel", 2, NOVALUE, NOVALUE, rastackrel},
    {"\\relstack", 1, NOVALUE, NOVALUE, rastackrel},
    {"\\sqrt", NOVALUE, NOVALUE, NOVALUE, rastsqrt},
    {"\\overbrace", OVERBRACE, 1, 1, rastaccent},
    {"\\underbrace", UNDERBRACE, 0, 1, rastaccent},
    {"\\overline", BARACCENT, 1, 0, rastaccent},
    {"\\underline", UNDERBARACCENT, 0, 0, rastaccent},
    {"\\begin", NOVALUE, NOVALUE, NOVALUE, rastbegin},
    {"\\array", NOVALUE, NOVALUE, NOVALUE, rastarray},
    {"\\matrix", NOVALUE, NOVALUE, NOVALUE, rastarray},
    {"\\tabular", NOVALUE, NOVALUE, NOVALUE, rastarray},
    {"\\picture", NOVALUE, NOVALUE, NOVALUE, rastpicture},
    {"\\line", NOVALUE, NOVALUE, NOVALUE, rastline},
    {"\\rule", NOVALUE, NOVALUE, NOVALUE, rastrule},
    {"\\circle", NOVALUE, NOVALUE, NOVALUE, rastcircle},
    {"\\bezier", NOVALUE, NOVALUE, NOVALUE, rastbezier},
    {"\\qbezier", NOVALUE, NOVALUE, NOVALUE, rastbezier},
    {"\\raisebox", NOVALUE, NOVALUE, NOVALUE, rastraise},
    {"\\rotatebox", NOVALUE, NOVALUE, NOVALUE, rastrotate},
    {"\\magnify", NOVALUE, NOVALUE, NOVALUE, rastmagnify},
    {"\\magbox", NOVALUE, NOVALUE, NOVALUE, rastmagnify},
    {"\\reflectbox", NOVALUE, NOVALUE, NOVALUE, rastreflect},
    {"\\fbox", NOVALUE, NOVALUE, NOVALUE, rastfbox},
    {"\\boxed", NOVALUE, NOVALUE, NOVALUE, rastfbox},
//    { "\\input",NOVALUE,NOVALUE,NOVALUE,  rastinput },
    {"\\evaluate", NOVALUE, NOVALUE, NOVALUE, rasteval},
//    { "\\today",NOVALUE,NOVALUE,NOVALUE,  rasttoday },
//    { "\\calendar",NOVALUE,NOVALUE,NOVALUE,rastcalendar },
//    { "\\environment",NOVALUE,NOVALUE,NOVALUE,rastenviron },
//    { "\\message",NOVALUE,NOVALUE,NOVALUE,rastmessage },
//    { "\\counter",NOVALUE,NOVALUE,NOVALUE,rastcounter },
    /* --- spaces --- */
    {"\\/", 1, NOVALUE, NOVALUE, rastspace},
    {"\\,", 2, NOVALUE, NOVALUE, rastspace},
    {"\\:", 4, NOVALUE, NOVALUE, rastspace},
    {"\\;", 6, NOVALUE, NOVALUE, rastspace},
    {"\\\n", 3, NOVALUE, NOVALUE, rastspace},
    {"\\\r", 3, NOVALUE, NOVALUE, rastspace},
    {"\\\t", 3, NOVALUE, NOVALUE, rastspace},
    /*{ "\\~",5,NOVALUE,NOVALUE,rastspace }, */
    {"~", 5, NOVALUE, NOVALUE, rastspace},
    {"\\ ", 5, NOVALUE, NOVALUE, rastspace},
    {" ", 5, NOVALUE, NOVALUE, rastspace},
    {"\\!", -2, NOVALUE, NOVALUE, rastspace},
    /*{ "\\!*", -2,      99,NOVALUE,  rastspace }, */
    {"\\quad", 6, NOVALUE, NOVALUE, rastspace},
    {"\\qquad", 10, NOVALUE, NOVALUE, rastspace},
    {"\\hspace", 0, NOVALUE, NOVALUE, rastspace},
    {"\\hspace*", 0, 99, NOVALUE, rastspace},
    {"\\vspace", 0, NOVALUE, 1, rastspace},
    {"\\hfill", 0, 1, NOVALUE, rastspace},
    /* --- newline --- */
    {"\\\\", NOVALUE, NOVALUE, NOVALUE, rastnewline},
    /* --- arrows --- */
    {"\\longrightarrow", 1, 0, NOVALUE, rastarrow},
    {"\\Longrightarrow", 1, 1, NOVALUE, rastarrow},
    {"\\longleftarrow", -1, 0, NOVALUE, rastarrow},
    {"\\Longleftarrow", -1, 1, NOVALUE, rastarrow},
    {"\\longleftrightarrow", 0, 0, NOVALUE, rastarrow},
    {"\\Longleftrightarrow", 0, 1, NOVALUE, rastarrow},
    {"\\longuparrow", 1, 0, NOVALUE, rastuparrow},
    {"\\Longuparrow", 1, 1, NOVALUE, rastuparrow},
    {"\\longdownarrow", -1, 0, NOVALUE, rastuparrow},
    {"\\Longdownarrow", -1, 1, NOVALUE, rastuparrow},
    {"\\longupdownarrow", 0, 0, NOVALUE, rastuparrow},
    {"\\Longupdownarrow", 0, 1, NOVALUE, rastuparrow},
    /* --- modes and values --- */
    {"\\cal", 1, NOVALUE, NOVALUE, rastfont},
    {"\\mathcal", 1, NOVALUE, NOVALUE, rastfont},
    {"\\scr", 2, NOVALUE, NOVALUE, rastfont},
    {"\\mathscr", 2, NOVALUE, NOVALUE, rastfont},
    {"\\mathfrak", 2, NOVALUE, NOVALUE, rastfont},
    {"\\mathbb", 5, NOVALUE, NOVALUE, rastfont},
    {"\\rm", 3, NOVALUE, NOVALUE, rastfont},
    {"\\text", 3, NOVALUE, NOVALUE, rastfont},
    {"\\textbf", 3, NOVALUE, NOVALUE, rastfont},
    {"\\textrm", 3, NOVALUE, NOVALUE, rastfont},
    {"\\mathrm", 7, NOVALUE, NOVALUE, rastfont},
    {"\\cyr", 8, NOVALUE, NOVALUE, rastfont},
    {"\\textgreek", 9, NOVALUE, NOVALUE, rastfont},
    {"\\textbfgreek", 10, NOVALUE, NOVALUE, rastfont},
    {"\\textbbgreek", 11, NOVALUE, NOVALUE, rastfont},
    {"\\mathbf", 6, NOVALUE, NOVALUE, rastfont},
    {"\\bf", 6, NOVALUE, NOVALUE, rastfont},
    {"\\mathtt", 3, NOVALUE, NOVALUE, rastfont},
    {"\\mathsf", 3, NOVALUE, NOVALUE, rastfont},
    {"\\mbox", 3, NOVALUE, NOVALUE, rastfont},
    {"\\operatorname", 3, NOVALUE, NOVALUE, rastfont},
    {"\\it", 4, NOVALUE, NOVALUE, rastfont},
    {"\\textit", 4, NOVALUE, NOVALUE, rastfont},
    {"\\mathit", 4, NOVALUE, NOVALUE, rastfont},
    {"\\rm", ISFONTFAM, 3, NOVALUE, rastflags},
    {"\\it", ISFONTFAM, 4, NOVALUE, rastflags},
    {"\\sl", ISFONTFAM, 4, NOVALUE, rastflags},
    {"\\bb", ISFONTFAM, 5, NOVALUE, rastflags},
    {"\\bf", ISFONTFAM, 6, NOVALUE, rastflags},
    {"\\text", ISFONTFAM, 3, NOVALUE, rastflags},
    {"\\math", ISFONTFAM, 0, NOVALUE, rastflags},
    {"\\ascii", ISSTRING, 1, NOVALUE, rastflags},
    {"\\image", ISSTRING, 0, NOVALUE, rastflags},
    {"\\limits", ISDISPLAYSTYLE, 2, NOVALUE, rastflags},
    {"\\nolimits", ISDISPLAYSTYLE, 0, NOVALUE, rastflags},
    {"\\displaystyle", ISDISPLAYSTYLE, 2, NOVALUE, rastflags},
    {"\\textstyle", ISDISPLAYSTYLE, 0, NOVALUE, rastflags},
    {"\\displaysize", ISDISPLAYSIZE, NOVALUE, NOVALUE, rastflags},
    {"\\tiny", ISFONTSIZE, 0, NOVALUE, rastflags},
    {"\\scriptsize", ISFONTSIZE, 1, NOVALUE, rastflags},
    {"\\footnotesize", ISFONTSIZE, 2, NOVALUE, rastflags},
    {"\\small", ISFONTSIZE, 3, NOVALUE, rastflags},
    {"\\normalsize", ISFONTSIZE, 4, NOVALUE, rastflags},
    {"\\large", ISFONTSIZE, 5, NOVALUE, rastflags},
    {"\\Large", ISFONTSIZE, 6, NOVALUE, rastflags},
    {"\\LARGE", ISFONTSIZE, 7, NOVALUE, rastflags},
    {"\\huge", ISFONTSIZE, 8, NOVALUE, rastflags},
    {"\\Huge", ISFONTSIZE, 9, NOVALUE, rastflags},
    {"\\HUGE", ISFONTSIZE, 10, NOVALUE, rastflags},
    {"\\fontsize", ISFONTSIZE, NOVALUE, NOVALUE, rastflags},
    {"\\fs", ISFONTSIZE, NOVALUE, NOVALUE, rastflags},
    {"\\magstep", ISMAGSTEP, NOVALUE, NOVALUE, rastflags},
    {"\\shrinkfactor", ISSHRINK, NOVALUE, NOVALUE, rastflags},
    {"\\sf", ISSHRINK, NOVALUE, NOVALUE, rastflags},
    {"\\light", ISWEIGHT, 0, NOVALUE, rastflags},
    {"\\regular", ISWEIGHT, 1, NOVALUE, rastflags},
    {"\\semibold", ISWEIGHT, 2, NOVALUE, rastflags},
    {"\\bold", ISWEIGHT, 3, NOVALUE, rastflags},
    {"\\fontweight", ISWEIGHT, NOVALUE, NOVALUE, rastflags},
    {"\\fw", ISWEIGHT, NOVALUE, NOVALUE, rastflags},
    {"\\centerwt", ISCENTERWT, NOVALUE, NOVALUE, rastflags},
    {"\\adjacentwt", ISADJACENTWT, NOVALUE, NOVALUE, rastflags},
    {"\\cornerwt", ISCORNERWT, NOVALUE, NOVALUE, rastflags},
    {"\\ssampling", ISSUPER, 1, NOVALUE, rastflags},
    {"\\lowpass", ISSUPER, 0, NOVALUE, rastflags},
    {"\\aaalg", ISAAALGORITHM, NOVALUE, NOVALUE, rastflags},
//    { "\\pnmparams",PNMPARAMS,   NOVALUE,NOVALUE, rastflags },
//    { "\\pbmpgm",    ISPBMPGM,   NOVALUE,NOVALUE, rastflags },
//    { "\\gammacorrection",ISGAMMA,NOVALUE,NOVALUE,rastflags },
//    { "\\nocontenttype",ISCONTENTTYPE, 0,NOVALUE, rastflags },
//    { "\\nodepth",   ISCONTENTCACHED,  0,NOVALUE, rastflags },
//    { "\\depth",     ISCONTENTCACHED,  1,NOVALUE, rastflags },
//    { "\\opaque",    ISOPAQUE,         0,NOVALUE, rastflags },
//    { "\\transparent",ISOPAQUE,        1,NOVALUE, rastflags },
    {"\\squash", ISSMASH, 3, 1, rastflags},
    {"\\smash", ISSMASH, 3, 1, rastflags},
    {"\\nosquash", ISSMASH, 0, NOVALUE, rastflags},
    {"\\nosmash", ISSMASH, 0, NOVALUE, rastflags},
    {"\\squashmargin", ISSMASH, NOVALUE, NOVALUE, rastflags},
    {"\\smashmargin", ISSMASH, NOVALUE, NOVALUE, rastflags},
    {"\\unitlength", UNITLENGTH, NOVALUE, NOVALUE, rastflags},
    {"\\reverse", ISREVERSE, NOVALUE, NOVALUE, rastflags},
    {"\\reversefg", ISREVERSE, 1, NOVALUE, rastflags},
    {"\\reversebg", ISREVERSE, 2, NOVALUE, rastflags},
//    { "\\color",     ISCOLOR,    NOVALUE,NOVALUE, rastflags },
//    { "\\red",       ISCOLOR,          1,NOVALUE, rastflags },
//    { "\\green",     ISCOLOR,          2,NOVALUE, rastflags },
//    { "\\blue",      ISCOLOR,          3,NOVALUE, rastflags },
//    { "\\black",     ISCOLOR,          0,NOVALUE, rastflags },
//    { "\\white",     ISCOLOR,          7,NOVALUE, rastflags },
    /* --- accents --- */
    {"\\vec", VECACCENT, 1, 1, rastaccent},
    {"\\widevec", VECACCENT, 1, 1, rastaccent},
    {"\\overarrow", VECACCENT, 1, 1, rastaccent},
    {"\\overrightarrow", VECACCENT, 1, 1, rastaccent},
    {"\\Overrightarrow", VECACCENT, 1, 11, rastaccent},
    {"\\underarrow", VECACCENT, 0, 1, rastaccent},
    {"\\underrightarrow", VECACCENT, 0, 1, rastaccent},
    {"\\Underrightarrow", VECACCENT, 0, 11, rastaccent},
    {"\\overleftarrow", VECACCENT, 1, -1, rastaccent},
    {"\\Overleftarrow", VECACCENT, 1, 9, rastaccent},
    {"\\underleftarrow", VECACCENT, 0, -1, rastaccent},
    {"\\Underleftarrow", VECACCENT, 0, 9, rastaccent},
    {"\\overleftrightarrow", VECACCENT, 1, 0, rastaccent},
    {"\\Overleftrightarrow", VECACCENT, 1, 10, rastaccent},
    {"\\underleftrightarrow", VECACCENT, 0, 0, rastaccent},
    {"\\Underleftrightarrow", VECACCENT, 0, 10, rastaccent},
    {"\\bar", BARACCENT, 1, 0, rastaccent},
    {"\\widebar", BARACCENT, 1, 0, rastaccent},
    {"\\hat", HATACCENT, 1, 0, rastaccent},
    {"\\widehat", HATACCENT, 1, 0, rastaccent},
    {"\\tilde", TILDEACCENT, 1, 0, rastaccent},
    {"\\widetilde", TILDEACCENT, 1, 0, rastaccent},
    {"\\dot", DOTACCENT, 1, 0, rastaccent},
    {"\\widedot", DOTACCENT, 1, 0, rastaccent},
    {"\\ddot", DDOTACCENT, 1, 0, rastaccent},
    {"\\wideddot", DDOTACCENT, 1, 0, rastaccent},
    /* --- math functions --- */
    {"\\arccos", 1, 0, NOVALUE, rastmathfunc},
    {"\\arcsin", 2, 0, NOVALUE, rastmathfunc},
    {"\\arctan", 3, 0, NOVALUE, rastmathfunc},
    {"\\arg", 4, 0, NOVALUE, rastmathfunc},
    {"\\cos", 5, 0, NOVALUE, rastmathfunc},
    {"\\cosh", 6, 0, NOVALUE, rastmathfunc},
    {"\\cot", 7, 0, NOVALUE, rastmathfunc},
    {"\\coth", 8, 0, NOVALUE, rastmathfunc},
    {"\\csc", 9, 0, NOVALUE, rastmathfunc},
    {"\\deg", 10, 0, NOVALUE, rastmathfunc},
    {"\\det", 11, 1, NOVALUE, rastmathfunc},
    {"\\dim", 12, 0, NOVALUE, rastmathfunc},
    {"\\exp", 13, 0, NOVALUE, rastmathfunc},
    {"\\gcd", 14, 1, NOVALUE, rastmathfunc},
    {"\\hom", 15, 0, NOVALUE, rastmathfunc},
    {"\\inf", 16, 1, NOVALUE, rastmathfunc},
    {"\\ker", 17, 0, NOVALUE, rastmathfunc},
    {"\\lg", 18, 0, NOVALUE, rastmathfunc},
    {"\\lim", 19, 1, NOVALUE, rastmathfunc},
    {"\\liminf", 20, 1, NOVALUE, rastmathfunc},
    {"\\limsup", 21, 1, NOVALUE, rastmathfunc},
    {"\\ln", 22, 0, NOVALUE, rastmathfunc},
    {"\\log", 23, 0, NOVALUE, rastmathfunc},
    {"\\max", 24, 1, NOVALUE, rastmathfunc},
    {"\\min", 25, 1, NOVALUE, rastmathfunc},
    {"\\Pr", 26, 1, NOVALUE, rastmathfunc},
    {"\\sec", 27, 0, NOVALUE, rastmathfunc},
    {"\\sin", 28, 0, NOVALUE, rastmathfunc},
    {"\\sinh", 29, 0, NOVALUE, rastmathfunc},
    {"\\sup", 30, 1, NOVALUE, rastmathfunc},
    {"\\tan", 31, 0, NOVALUE, rastmathfunc},
    {"\\tanh", 32, 0, NOVALUE, rastmathfunc},
    {"\\tr", 33, 0, NOVALUE, rastmathfunc},
    {"\\pmod", 34, 0, NOVALUE, rastmathfunc},
    /* --- flush -- recognized but not yet handled by mimeTeX --- */
    {"\\nooperation", 0, NOVALUE, NOVALUE, rastnoop},
    {"\\bigskip", 0, NOVALUE, NOVALUE, rastnoop},
    {"\\phantom", 1, NOVALUE, NOVALUE, rastnoop},
    {"\\nocaching", 0, NOVALUE, NOVALUE, rastnoop},
    {"\\noconten", 0, NOVALUE, NOVALUE, rastnoop},
    {"\\nonumber", 0, NOVALUE, NOVALUE, rastnoop},
    /* { "\\!",      0, NOVALUE,NOVALUE,  rastnoop }, */
    {"\\cydot", 0, NOVALUE, NOVALUE, rastnoop},
    /* --------------------- C M M I --------------------------
       symbol     charnum    family    class     function
       -------------------------------------------------------- */
    /* --- uppercase greek letters --- */
    {"\\Gamma", 0, CMMI10, VARIABLE, NULL},
    {"\\Delta", 1, CMMI10, VARIABLE, NULL},
    {"\\Theta", 2, CMMI10, VARIABLE, NULL},
    {"\\Lambda", 3, CMMI10, VARIABLE, NULL},
    {"\\Xi", 4, CMMI10, VARIABLE, NULL},
    {"\\Pi", 5, CMMI10, VARIABLE, NULL},
    {"\\Sigma", 6, CMMI10, VARIABLE, NULL},
    {"\\smallsum", 6, CMMI10, OPERATOR, NULL},
    {"\\Upsilon", 7, CMMI10, VARIABLE, NULL},
    {"\\Phi", 8, CMMI10, VARIABLE, NULL},
    {"\\Psi", 9, CMMI10, VARIABLE, NULL},
    {"\\Omega", 10, CMMI10, VARIABLE, NULL},
    /* --- lowercase greek letters --- */
    {"\\alpha", 11, CMMI10, VARIABLE, NULL},
    {"\\beta", 12, CMMI10, VARIABLE, NULL},
    {"\\gamma", 13, CMMI10, VARIABLE, NULL},
    {"\\delta", 14, CMMI10, VARIABLE, NULL},
    {"\\epsilon", 15, CMMI10, VARIABLE, NULL},
    {"\\zeta", 16, CMMI10, VARIABLE, NULL},
    {"\\eta", 17, CMMI10, VARIABLE, NULL},
    {"\\theta", 18, CMMI10, VARIABLE, NULL},
    {"\\iota", 19, CMMI10, VARIABLE, NULL},
    {"\\kappa", 20, CMMI10, VARIABLE, NULL},
    {"\\lambda", 21, CMMI10, VARIABLE, NULL},
    {"\\mu", 22, CMMI10, VARIABLE, NULL},
    {"\\nu", 23, CMMI10, VARIABLE, NULL},
    {"\\xi", 24, CMMI10, VARIABLE, NULL},
    {"\\pi", 25, CMMI10, VARIABLE, NULL},
    {"\\rho", 26, CMMI10, VARIABLE, NULL},
    {"\\sigma", 27, CMMI10, VARIABLE, NULL},
    {"\\tau", 28, CMMI10, VARIABLE, NULL},
    {"\\upsilon", 29, CMMI10, VARIABLE, NULL},
    {"\\phi", 30, CMMI10, VARIABLE, NULL},
    {"\\chi", 31, CMMI10, VARIABLE, NULL},
    {"\\psi", 32, CMMI10, VARIABLE, NULL},
    {"\\omega", 33, CMMI10, VARIABLE, NULL},
    {"\\varepsilon", 34, CMMI10, VARIABLE, NULL},
    {"\\vartheta", 35, CMMI10, VARIABLE, NULL},
    {"\\varpi", 36, CMMI10, VARIABLE, NULL},
    {"\\varrho", 37, CMMI10, VARIABLE, NULL},
    {"\\varsigma", 38, CMMI10, VARIABLE, NULL},
    {"\\varphi", 39, CMMI10, VARIABLE, NULL},
    /* --- arrow relations --- */
    {"\\leftharpoonup", 40, CMMI10, ARROW, NULL},
    {"\\leftharpoondown", 41, CMMI10, ARROW, NULL},
    {"\\rightharpoonup", 42, CMMI10, ARROW, NULL},
    {"\\rightharpoondown", 43, CMMI10, ARROW, NULL},
    /* --- punctuation --- */
    {"`", 44, CMMI10, PUNCTION, NULL},
    {"\'", 45, CMMI10, PUNCTION, NULL},
    /* --- triangle binary relations --- */
    {"\\triangleright", 46, CMMI10, RELATION, NULL},
    {"\\triangleleft", 47, CMMI10, RELATION, NULL},
    /* --- digits 0-9 --- */
    {"\\0", 48, CMMI10, ORDINARY, NULL},
    {"\\1", 49, CMMI10, ORDINARY, NULL},
    {"\\2", 50, CMMI10, ORDINARY, NULL},
    {"\\3", 51, CMMI10, ORDINARY, NULL},
    {"\\4", 52, CMMI10, ORDINARY, NULL},
    {"\\5", 53, CMMI10, ORDINARY, NULL},
    {"\\6", 54, CMMI10, ORDINARY, NULL},
    {"\\7", 55, CMMI10, ORDINARY, NULL},
    {"\\8", 56, CMMI10, ORDINARY, NULL},
    {"\\9", 57, CMMI10, ORDINARY, NULL},
    /* --- punctuation --- */
    {".", 58, CMMI10, PUNCTION, NULL},
    {",", 59, CMMI10, PUNCTION, NULL},
    /* --- operations (some ordinary) --- */
    {"<", 60, CMMI10, OPENING, NULL},
    {"\\<", 60, CMMI10, OPENING, NULL},
    {"\\lt", 60, CMMI10, OPENING, NULL},
    {"/", 61, CMMI10, BINARYOP, NULL},
    {">", 62, CMMI10, CLOSING, NULL},
    {"\\>", 62, CMMI10, CLOSING, NULL},
    {"\\gt", 62, CMMI10, CLOSING, NULL},
    {"\\star", 63, CMMI10, BINARYOP, NULL},
    {"\\partial", 64, CMMI10, VARIABLE, NULL},
    /* --- uppercase letters --- */
    {"A", 65, CMMI10, VARIABLE, NULL},
    {"B", 66, CMMI10, VARIABLE, NULL},
    {"C", 67, CMMI10, VARIABLE, NULL},
    {"D", 68, CMMI10, VARIABLE, NULL},
    {"E", 69, CMMI10, VARIABLE, NULL},
    {"F", 70, CMMI10, VARIABLE, NULL},
    {"G", 71, CMMI10, VARIABLE, NULL},
    {"H", 72, CMMI10, VARIABLE, NULL},
    {"I", 73, CMMI10, VARIABLE, NULL},
    {"J", 74, CMMI10, VARIABLE, NULL},
    {"K", 75, CMMI10, VARIABLE, NULL},
    {"L", 76, CMMI10, VARIABLE, NULL},
    {"M", 77, CMMI10, VARIABLE, NULL},
    {"N", 78, CMMI10, VARIABLE, NULL},
    {"O", 79, CMMI10, VARIABLE, NULL},
    {"P", 80, CMMI10, VARIABLE, NULL},
    {"Q", 81, CMMI10, VARIABLE, NULL},
    {"R", 82, CMMI10, VARIABLE, NULL},
    {"S", 83, CMMI10, VARIABLE, NULL},
    {"T", 84, CMMI10, VARIABLE, NULL},
    {"U", 85, CMMI10, VARIABLE, NULL},
    {"V", 86, CMMI10, VARIABLE, NULL},
    {"W", 87, CMMI10, VARIABLE, NULL},
    {"X", 88, CMMI10, VARIABLE, NULL},
    {"Y", 89, CMMI10, VARIABLE, NULL},
    {"Z", 90, CMMI10, VARIABLE, NULL},
    /* --- miscellaneous symbols and relations --- */
    {"\\flat", 91, CMMI10, ORDINARY, NULL},
    {"\\natural", 92, CMMI10, ORDINARY, NULL},
    {"\\sharp", 93, CMMI10, ORDINARY, NULL},
    {"\\smile", 94, CMMI10, RELATION, NULL},
    {"\\frown", 95, CMMI10, RELATION, NULL},
    {"\\ell", 96, CMMI10, ORDINARY, NULL},
    /* --- lowercase letters --- */
    {"a", 97, CMMI10, VARIABLE, NULL},
    {"b", 98, CMMI10, VARIABLE, NULL},
    {"c", 99, CMMI10, VARIABLE, NULL},
    {"d", 100, CMMI10, VARIABLE, NULL},
    {"e", 101, CMMI10, VARIABLE, NULL},
    {"f", 102, CMMI10, VARIABLE, NULL},
    {"g", 103, CMMI10, VARIABLE, NULL},
    {"h", 104, CMMI10, VARIABLE, NULL},
    {"i", 105, CMMI10, VARIABLE, NULL},
    {"j", 106, CMMI10, VARIABLE, NULL},
    {"k", 107, CMMI10, VARIABLE, NULL},
    {"l", 108, CMMI10, VARIABLE, NULL},
    {"m", 109, CMMI10, VARIABLE, NULL},
    {"n", 110, CMMI10, VARIABLE, NULL},
    {"o", 111, CMMI10, VARIABLE, NULL},
    {"p", 112, CMMI10, VARIABLE, NULL},
    {"q", 113, CMMI10, VARIABLE, NULL},
    {"r", 114, CMMI10, VARIABLE, NULL},
    {"s", 115, CMMI10, VARIABLE, NULL},
    {"t", 116, CMMI10, VARIABLE, NULL},
    {"u", 117, CMMI10, VARIABLE, NULL},
    {"v", 118, CMMI10, VARIABLE, NULL},
    {"w", 119, CMMI10, VARIABLE, NULL},
    {"x", 120, CMMI10, VARIABLE, NULL},
    {"y", 121, CMMI10, VARIABLE, NULL},
    {"z", 122, CMMI10, VARIABLE, NULL},
    /* --- miscellaneous symbols and relations --- */
    {"\\imath", 123, CMMI10, VARIABLE, NULL},
    {"\\jmath", 124, CMMI10, VARIABLE, NULL},
    {"\\wp", 125, CMMI10, ORDINARY, NULL},
    {"\\vec", 126, CMMI10, ORDINARY, NULL},
    /* --------------------- C M M I B ------------------------
       symbol     charnum    family    class     function
       -------------------------------------------------------- */
    /* --- uppercase greek letters --- */
    {"\\Gamma", 0, CMMIB10, VARIABLE, NULL},
    {"\\Delta", 1, CMMIB10, VARIABLE, NULL},
    {"\\Theta", 2, CMMIB10, VARIABLE, NULL},
    {"\\Lambda", 3, CMMIB10, VARIABLE, NULL},
    {"\\Xi", 4, CMMIB10, VARIABLE, NULL},
    {"\\Pi", 5, CMMIB10, VARIABLE, NULL},
    {"\\Sigma", 6, CMMIB10, VARIABLE, NULL},
    {"\\smallsum", 6, CMMIB10, OPERATOR, NULL},
    {"\\Upsilon", 7, CMMIB10, VARIABLE, NULL},
    {"\\Phi", 8, CMMIB10, VARIABLE, NULL},
    {"\\Psi", 9, CMMIB10, VARIABLE, NULL},
    {"\\Omega", 10, CMMIB10, VARIABLE, NULL},
    /* --- lowercase greek letters --- */
    {"\\alpha", 11, CMMIB10, VARIABLE, NULL},
    {"\\beta", 12, CMMIB10, VARIABLE, NULL},
    {"\\gamma", 13, CMMIB10, VARIABLE, NULL},
    {"\\delta", 14, CMMIB10, VARIABLE, NULL},
    {"\\epsilon", 15, CMMIB10, VARIABLE, NULL},
    {"\\zeta", 16, CMMIB10, VARIABLE, NULL},
    {"\\eta", 17, CMMIB10, VARIABLE, NULL},
    {"\\theta", 18, CMMIB10, VARIABLE, NULL},
    {"\\iota", 19, CMMIB10, VARIABLE, NULL},
    {"\\kappa", 20, CMMIB10, VARIABLE, NULL},
    {"\\lambda", 21, CMMIB10, VARIABLE, NULL},
    {"\\mu", 22, CMMIB10, VARIABLE, NULL},
    {"\\nu", 23, CMMIB10, VARIABLE, NULL},
    {"\\xi", 24, CMMIB10, VARIABLE, NULL},
    {"\\pi", 25, CMMIB10, VARIABLE, NULL},
    {"\\rho", 26, CMMIB10, VARIABLE, NULL},
    {"\\sigma", 27, CMMIB10, VARIABLE, NULL},
    {"\\tau", 28, CMMIB10, VARIABLE, NULL},
    {"\\upsilon", 29, CMMIB10, VARIABLE, NULL},
    {"\\phi", 30, CMMIB10, VARIABLE, NULL},
    {"\\chi", 31, CMMIB10, VARIABLE, NULL},
    {"\\psi", 32, CMMIB10, VARIABLE, NULL},
    {"\\omega", 33, CMMIB10, VARIABLE, NULL},
    {"\\varepsilon", 34, CMMIB10, VARIABLE, NULL},
    {"\\vartheta", 35, CMMIB10, VARIABLE, NULL},
    {"\\varpi", 36, CMMIB10, VARIABLE, NULL},
    {"\\varrho", 37, CMMIB10, VARIABLE, NULL},
    {"\\varsigma", 38, CMMIB10, VARIABLE, NULL},
    {"\\varphi", 39, CMMIB10, VARIABLE, NULL},
    /* --- arrow relations --- */
    {"\\bfleftharpoonup", 40, CMMIB10, ARROW, NULL},
    {"\\bfleftharpoondown", 41, CMMIB10, ARROW, NULL},
    {"\\bfrightharpoonup", 42, CMMIB10, ARROW, NULL},
    {"\\bfrightharpoondown", 43, CMMIB10, ARROW, NULL},
    /* --- punctuation --- */
    {"`", 44, CMMIB10, PUNCTION, NULL},
    {"\'", 45, CMMIB10, PUNCTION, NULL},
    /* --- triangle binary relations --- */
    {"\\triangleright", 46, CMMIB10, RELATION, NULL},
    {"\\triangleleft", 47, CMMIB10, RELATION, NULL},
    /* --- digits 0-9 --- */
    {"\\0", 48, CMMIB10, ORDINARY, NULL},
    {"\\1", 49, CMMIB10, ORDINARY, NULL},
    {"\\2", 50, CMMIB10, ORDINARY, NULL},
    {"\\3", 51, CMMIB10, ORDINARY, NULL},
    {"\\4", 52, CMMIB10, ORDINARY, NULL},
    {"\\5", 53, CMMIB10, ORDINARY, NULL},
    {"\\6", 54, CMMIB10, ORDINARY, NULL},
    {"\\7", 55, CMMIB10, ORDINARY, NULL},
    {"\\8", 56, CMMIB10, ORDINARY, NULL},
    {"\\9", 57, CMMIB10, ORDINARY, NULL},
    /* --- punctuation --- */
    {".", 58, CMMIB10, PUNCTION, NULL},
    {",", 59, CMMIB10, PUNCTION, NULL},
    /* --- operations (some ordinary) --- */
    {"<", 60, CMMIB10, OPENING, NULL},
    {"\\lt", 60, CMMIB10, OPENING, NULL},
    {"/", 61, CMMIB10, BINARYOP, NULL},
    {">", 62, CMMIB10, CLOSING, NULL},
    {"\\gt", 62, CMMIB10, CLOSING, NULL},
    {"\\star", 63, CMMIB10, BINARYOP, NULL},
    {"\\partial", 64, CMMIB10, VARIABLE, NULL},
    /* --- uppercase letters --- */
    {"A", 65, CMMIB10, VARIABLE, NULL},
    {"B", 66, CMMIB10, VARIABLE, NULL},
    {"C", 67, CMMIB10, VARIABLE, NULL},
    {"D", 68, CMMIB10, VARIABLE, NULL},
    {"E", 69, CMMIB10, VARIABLE, NULL},
    {"F", 70, CMMIB10, VARIABLE, NULL},
    {"G", 71, CMMIB10, VARIABLE, NULL},
    {"H", 72, CMMIB10, VARIABLE, NULL},
    {"I", 73, CMMIB10, VARIABLE, NULL},
    {"J", 74, CMMIB10, VARIABLE, NULL},
    {"K", 75, CMMIB10, VARIABLE, NULL},
    {"L", 76, CMMIB10, VARIABLE, NULL},
    {"M", 77, CMMIB10, VARIABLE, NULL},
    {"N", 78, CMMIB10, VARIABLE, NULL},
    {"O", 79, CMMIB10, VARIABLE, NULL},
    {"P", 80, CMMIB10, VARIABLE, NULL},
    {"Q", 81, CMMIB10, VARIABLE, NULL},
    {"R", 82, CMMIB10, VARIABLE, NULL},
    {"S", 83, CMMIB10, VARIABLE, NULL},
    {"T", 84, CMMIB10, VARIABLE, NULL},
    {"U", 85, CMMIB10, VARIABLE, NULL},
    {"V", 86, CMMIB10, VARIABLE, NULL},
    {"W", 87, CMMIB10, VARIABLE, NULL},
    {"X", 88, CMMIB10, VARIABLE, NULL},
    {"Y", 89, CMMIB10, VARIABLE, NULL},
    {"Z", 90, CMMIB10, VARIABLE, NULL},
    /* --- miscellaneous symbols and relations --- */
    {"\\flat", 91, CMMIB10, ORDINARY, NULL},
    {"\\natural", 92, CMMIB10, ORDINARY, NULL},
    {"\\sharp", 93, CMMIB10, ORDINARY, NULL},
    {"\\smile", 94, CMMIB10, RELATION, NULL},
    {"\\frown", 95, CMMIB10, RELATION, NULL},
    {"\\ell", 96, CMMIB10, ORDINARY, NULL},
    /* --- lowercase letters --- */
    {"a", 97, CMMIB10, VARIABLE, NULL},
    {"b", 98, CMMIB10, VARIABLE, NULL},
    {"c", 99, CMMIB10, VARIABLE, NULL},
    {"d", 100, CMMIB10, VARIABLE, NULL},
    {"e", 101, CMMIB10, VARIABLE, NULL},
    {"f", 102, CMMIB10, VARIABLE, NULL},
    {"g", 103, CMMIB10, VARIABLE, NULL},
    {"h", 104, CMMIB10, VARIABLE, NULL},
    {"i", 105, CMMIB10, VARIABLE, NULL},
    {"j", 106, CMMIB10, VARIABLE, NULL},
    {"k", 107, CMMIB10, VARIABLE, NULL},
    {"l", 108, CMMIB10, VARIABLE, NULL},
    {"m", 109, CMMIB10, VARIABLE, NULL},
    {"n", 110, CMMIB10, VARIABLE, NULL},
    {"o", 111, CMMIB10, VARIABLE, NULL},
    {"p", 112, CMMIB10, VARIABLE, NULL},
    {"q", 113, CMMIB10, VARIABLE, NULL},
    {"r", 114, CMMIB10, VARIABLE, NULL},
    {"s", 115, CMMIB10, VARIABLE, NULL},
    {"t", 116, CMMIB10, VARIABLE, NULL},
    {"u", 117, CMMIB10, VARIABLE, NULL},
    {"v", 118, CMMIB10, VARIABLE, NULL},
    {"w", 119, CMMIB10, VARIABLE, NULL},
    {"x", 120, CMMIB10, VARIABLE, NULL},
    {"y", 121, CMMIB10, VARIABLE, NULL},
    {"z", 122, CMMIB10, VARIABLE, NULL},
    /* --- miscellaneous symbols and relations --- */
    {"\\imath", 123, CMMIB10, VARIABLE, NULL},
    {"\\jmath", 124, CMMIB10, VARIABLE, NULL},
    {"\\wp", 125, CMMIB10, ORDINARY, NULL},
    {"\\bfvec", 126, CMMIB10, ORDINARY, NULL},
    /* --------------------- C M S Y --------------------------
       symbol     charnum    family    class     function
       -------------------------------------------------------- */
    /* --- operations --- */
    {"-", 0, CMSY10, BINARYOP, NULL},
    {"\\cdot", 1, CMSY10, BINARYOP, NULL},
    {"\\times", 2, CMSY10, BINARYOP, NULL},
    {"\\ast", 3, CMSY10, BINARYOP, NULL},
    {"\\div", 4, CMSY10, BINARYOP, NULL},
    {"\\diamond", 5, CMSY10, BINARYOP, NULL},
    {"\\pm", 6, CMSY10, BINARYOP, NULL},
    {"\\mp", 7, CMSY10, BINARYOP, NULL},
    {"\\oplus", 8, CMSY10, BINARYOP, NULL},
    {"\\ominus", 9, CMSY10, BINARYOP, NULL},
    {"\\otimes", 10, CMSY10, BINARYOP, NULL},
    {"\\oslash", 11, CMSY10, BINARYOP, NULL},
    {"\\odot", 12, CMSY10, BINARYOP, NULL},
    {"\\bigcirc", 13, CMSY10, BINARYOP, NULL},
    {"\\circ", 14, CMSY10, BINARYOP, NULL},
    {"\\bullet", 15, CMSY10, BINARYOP, NULL},
    /* --- relations --- */
    {"\\asymp", 16, CMSY10, RELATION, NULL},
    {"\\equiv", 17, CMSY10, RELATION, NULL},
    {"\\subseteq", 18, CMSY10, RELATION, NULL},
    {"\\supseteq", 19, CMSY10, RELATION, NULL},
    {"\\leq", 20, CMSY10, RELATION, NULL},
    {"\\geq", 21, CMSY10, RELATION, NULL},
    {"\\preceq", 22, CMSY10, RELATION, NULL},
    {"\\succeq", 23, CMSY10, RELATION, NULL},
    {"\\sim", 24, CMSY10, RELATION, NULL},
    {"\\approx", 25, CMSY10, RELATION, NULL},
    {"\\subset", 26, CMSY10, RELATION, NULL},
    {"\\supset", 27, CMSY10, RELATION, NULL},
    {"\\ll", 28, CMSY10, RELATION, NULL},
    {"\\gg", 29, CMSY10, RELATION, NULL},
    {"\\prec", 30, CMSY10, RELATION, NULL},
    {"\\succ", 31, CMSY10, RELATION, NULL},
    /* --- (mostly) arrows --- */
    {"\\leftarrow", 32, CMSY10, ARROW, NULL},
    {"\\rightarrow", 33, CMSY10, ARROW, NULL},
    {"\\to", 33, CMSY10, ARROW, NULL},
    {"\\mapsto", 33, CMSY10, ARROW, NULL},
    {"\\uparrow", 34, CMSY10, ARROW, NULL},
    {"\\downarrow", 35, CMSY10, ARROW, NULL},
    {"\\leftrightarrow", 36, CMSY10, ARROW, NULL},
    {"\\nearrow", 37, CMSY10, ARROW, NULL},
    {"\\searrow", 38, CMSY10, ARROW, NULL},
    {"\\simeq", 39, CMSY10, RELATION, NULL},
    {"\\Leftarrow", 40, CMSY10, ARROW, NULL},
    {"\\Rightarrow", 41, CMSY10, ARROW, NULL},
    {"\\Uparrow", 42, CMSY10, ARROW, NULL},
    {"\\Downarrow", 43, CMSY10, ARROW, NULL},
    {"\\Leftrightarrow", 44, CMSY10, ARROW, NULL},
    {"\\nwarrow", 45, CMSY10, ARROW, NULL},
    {"\\swarrow", 46, CMSY10, ARROW, NULL},
    {"\\propto", 47, CMSY10, RELATION, NULL},
    /* --- symbols --- */
    {"\\prime", 48, CMSY10, ORDINARY, NULL},
    {"\\infty", 49, CMSY10, ORDINARY, NULL},
    /* --- relations --- */
    {"\\in", 50, CMSY10, RELATION, NULL},
    {"\\ni", 51, CMSY10, RELATION, NULL},
    /* --- symbols --- */
    {"\\triangle", 52, CMSY10, ORDINARY, NULL},
    {"\\bigtriangleup", 52, CMSY10, ORDINARY, NULL},
    {"\\bigtriangledown", 53, CMSY10, ORDINARY, NULL},
    {"\\boldslash", 54, CMSY10, BINARYOP, NULL},
    {"\\'", 55, CMSY10, ORDINARY, NULL},
    {"\\forall", 56, CMSY10, OPERATOR, NULL},
    {"\\exists", 57, CMSY10, OPERATOR, NULL},
    {"\\neg", 58, CMSY10, OPERATOR, NULL},
    {"\\emptyset", 59, CMSY10, ORDINARY, NULL},
    {"\\Re", 60, CMSY10, ORDINARY, NULL},
    {"\\Im", 61, CMSY10, ORDINARY, NULL},
    {"\\top", 62, CMSY10, ORDINARY, NULL},
    {"\\bot", 63, CMSY10, ORDINARY, NULL},
    {"\\perp", 63, CMSY10, BINARYOP, NULL},
    {"\\aleph", 64, CMSY10, ORDINARY, NULL},
    /* --- calligraphic letters (we use \\calA...\\calZ --- */
    {"\\calA", 65, CMSY10, VARIABLE, NULL},
    {"\\calB", 66, CMSY10, VARIABLE, NULL},
    {"\\calC", 67, CMSY10, VARIABLE, NULL},
    {"\\calD", 68, CMSY10, VARIABLE, NULL},
    {"\\calE", 69, CMSY10, VARIABLE, NULL},
    {"\\calF", 70, CMSY10, VARIABLE, NULL},
    {"\\calG", 71, CMSY10, VARIABLE, NULL},
    {"\\calH", 72, CMSY10, VARIABLE, NULL},
    {"\\calI", 73, CMSY10, VARIABLE, NULL},
    {"\\calJ", 74, CMSY10, VARIABLE, NULL},
    {"\\calK", 75, CMSY10, VARIABLE, NULL},
    {"\\calL", 76, CMSY10, VARIABLE, NULL},
    {"\\calM", 77, CMSY10, VARIABLE, NULL},
    {"\\calN", 78, CMSY10, VARIABLE, NULL},
    {"\\calO", 79, CMSY10, VARIABLE, NULL},
    {"\\calP", 80, CMSY10, VARIABLE, NULL},
    {"\\calQ", 81, CMSY10, VARIABLE, NULL},
    {"\\calR", 82, CMSY10, VARIABLE, NULL},
    {"\\calS", 83, CMSY10, VARIABLE, NULL},
    {"\\calT", 84, CMSY10, VARIABLE, NULL},
    {"\\calU", 85, CMSY10, VARIABLE, NULL},
    {"\\calV", 86, CMSY10, VARIABLE, NULL},
    {"\\calW", 87, CMSY10, VARIABLE, NULL},
    {"\\calX", 88, CMSY10, VARIABLE, NULL},
    {"\\calY", 89, CMSY10, VARIABLE, NULL},
    {"\\calZ", 90, CMSY10, VARIABLE, NULL},
    {"A", 65, CMSY10, VARIABLE, NULL},
    {"B", 66, CMSY10, VARIABLE, NULL},
    {"C", 67, CMSY10, VARIABLE, NULL},
    {"D", 68, CMSY10, VARIABLE, NULL},
    {"E", 69, CMSY10, VARIABLE, NULL},
    {"F", 70, CMSY10, VARIABLE, NULL},
    {"G", 71, CMSY10, VARIABLE, NULL},
    {"H", 72, CMSY10, VARIABLE, NULL},
    {"I", 73, CMSY10, VARIABLE, NULL},
    {"J", 74, CMSY10, VARIABLE, NULL},
    {"K", 75, CMSY10, VARIABLE, NULL},
    {"L", 76, CMSY10, VARIABLE, NULL},
    {"M", 77, CMSY10, VARIABLE, NULL},
    {"N", 78, CMSY10, VARIABLE, NULL},
    {"O", 79, CMSY10, VARIABLE, NULL},
    {"P", 80, CMSY10, VARIABLE, NULL},
    {"Q", 81, CMSY10, VARIABLE, NULL},
    {"R", 82, CMSY10, VARIABLE, NULL},
    {"S", 83, CMSY10, VARIABLE, NULL},
    {"T", 84, CMSY10, VARIABLE, NULL},
    {"U", 85, CMSY10, VARIABLE, NULL},
    {"V", 86, CMSY10, VARIABLE, NULL},
    {"W", 87, CMSY10, VARIABLE, NULL},
    {"X", 88, CMSY10, VARIABLE, NULL},
    {"Y", 89, CMSY10, VARIABLE, NULL},
    {"Z", 90, CMSY10, VARIABLE, NULL},
    /* --- operations and relations --- */
    {"\\cup", 91, CMSY10, OPERATOR, NULL},
    {"\\cap", 92, CMSY10, OPERATOR, NULL},
    {"\\uplus", 93, CMSY10, OPERATOR, NULL},
    {"\\wedge", 94, CMSY10, OPERATOR, NULL},
    {"\\vee", 95, CMSY10, OPERATOR, NULL},
    {"\\vdash", 96, CMSY10, RELATION, NULL},
    {"\\dashv", 97, CMSY10, RELATION, NULL},
    /* --- brackets --- */
    {"\\lfloor", 98, CMSY10, OPENING, NULL},
    {"\\rfloor", 99, CMSY10, CLOSING, NULL},
    {"\\lceil", 100, CMSY10, OPENING, NULL},
    {"\\rceil", 101, CMSY10, CLOSING, NULL},
    {"\\lbrace", 102, CMSY10, OPENING, NULL},
    {"{", 102, CMSY10, OPENING, NULL},
    {"\\{", 102, CMSY10, OPENING, NULL},
    {"\\rbrace", 103, CMSY10, CLOSING, NULL},
    {"}", 103, CMSY10, CLOSING, NULL},
    {"\\}", 103, CMSY10, CLOSING, NULL},
    {"\\langle", 104, CMSY10, OPENING, NULL},
    {"\\rangle", 105, CMSY10, CLOSING, NULL},
    {"\\mid", 106, CMSY10, ORDINARY, NULL},
    {"|", 106, CMSY10, BINARYOP, NULL},
    {"\\parallel", 107, CMSY10, BINARYOP, NULL},
    {"\\|", 107, CMSY10, BINARYOP, NULL},
    /* --- arrows --- */
    {"\\updownarrow", 108, CMSY10, ARROW, NULL},
    {"\\Updownarrow", 109, CMSY10, ARROW, NULL},
    /* --- symbols and operations and relations --- */
    {"\\setminus", 110, CMSY10, BINARYOP, NULL},
    {"\\backslash", 110, CMSY10, BINARYOP, NULL},
    {"\\wr", 111, CMSY10, BINARYOP, NULL},
    {"\\surd", 112, CMSY10, OPERATOR, NULL},
    {"\\amalg", 113, CMSY10, BINARYOP, NULL},
    {"\\nabla", 114, CMSY10, VARIABLE, NULL},
    {"\\smallint", 115, CMSY10, OPERATOR, NULL},
    {"\\sqcup", 116, CMSY10, OPERATOR, NULL},
    {"\\sqcap", 117, CMSY10, OPERATOR, NULL},
    {"\\sqsubseteq", 118, CMSY10, RELATION, NULL},
    {"\\sqsupseteq", 119, CMSY10, RELATION, NULL},
    /* --- special characters --- */
    {"\\S", 120, CMSY10, ORDINARY, NULL},
    {"\\dag", 121, CMSY10, ORDINARY, NULL},
    {"\\dagger", 121, CMSY10, ORDINARY, NULL},
    {"\\ddag", 122, CMSY10, ORDINARY, NULL},
    {"\\ddagger", 122, CMSY10, ORDINARY, NULL},
    {"\\P", 123, CMSY10, ORDINARY, NULL},
    {"\\clubsuit", 124, CMSY10, ORDINARY, NULL},
    {"\\Diamond", 125, CMSY10, ORDINARY, NULL},
    {"\\Heart", 126, CMSY10, ORDINARY, NULL},
    {"\\spadesuit", 127, CMSY10, ORDINARY, NULL},
    /* ---------------------- C M R ---------------------------
       symbol     charnum    family    class     function
       -------------------------------------------------------- */
    /* --- uppercase greek letters --- */
    {"\\Gamma", 0, CMR10, VARIABLE, NULL},
    {"\\Delta", 1, CMR10, VARIABLE, NULL},
    {"\\Theta", 2, CMR10, VARIABLE, NULL},
    {"\\Lambda", 3, CMR10, VARIABLE, NULL},
    {"\\Xi", 4, CMR10, VARIABLE, NULL},
    {"\\Pi", 5, CMR10, VARIABLE, NULL},
    {"\\Sigma", 6, CMR10, VARIABLE, NULL},
    {"\\smallsum", 6, CMR10, OPERATOR, NULL},
    {"\\Upsilon", 7, CMR10, VARIABLE, NULL},
    {"\\Phi", 8, CMR10, VARIABLE, NULL},
    {"\\Psi", 9, CMR10, VARIABLE, NULL},
    {"\\Omega", 10, CMR10, VARIABLE, NULL},
    /* ---  --- */
    {"\\ff", 11, CMR10, ORDINARY, NULL},
    {"\\fi", 12, CMR10, ORDINARY, NULL},
    {"\\fl", 13, CMR10, ORDINARY, NULL},
    {"\\ffi", 14, CMR10, ORDINARY, NULL},
    {"\\ffl", 15, CMR10, ORDINARY, NULL},
    {"\\imath", 16, CMR10, ORDINARY, NULL},
    {"\\jmath", 17, CMR10, ORDINARY, NULL},
    /* --- foreign letters --- */
    {"\\ss", 25, CMR10, ORDINARY, NULL},
    {"\\ae", 26, CMR10, ORDINARY, NULL},
    {"\\oe", 27, CMR10, ORDINARY, NULL},
    {"\\AE", 29, CMR10, ORDINARY, NULL},
    {"\\OE", 30, CMR10, ORDINARY, NULL},
    /* --- digits 0-9 --- */
    {"0", 48, CMR10, ORDINARY, NULL},
    {"1", 49, CMR10, ORDINARY, NULL},
    {"2", 50, CMR10, ORDINARY, NULL},
    {"3", 51, CMR10, ORDINARY, NULL},
    {"4", 52, CMR10, ORDINARY, NULL},
    {"5", 53, CMR10, ORDINARY, NULL},
    {"6", 54, CMR10, ORDINARY, NULL},
    {"7", 55, CMR10, ORDINARY, NULL},
    {"8", 56, CMR10, ORDINARY, NULL},
    {"9", 57, CMR10, ORDINARY, NULL},
    /* --- symbols, relations, etc --- */
    {"\\gravesym", 18, CMR10, ORDINARY, NULL},
    {"\\acutesym", 19, CMR10, ORDINARY, NULL},
    {"\\checksym", 20, CMR10, ORDINARY, NULL},
    {"\\brevesym", 21, CMR10, ORDINARY, NULL},
    {"!", 33, CMR10, BINARYOP, NULL},
    {"\"", 34, CMR10, ORDINARY, NULL},
    {"\\quote", 34, CMR10, ORDINARY, NULL},
    {"#", 35, CMR10, BINARYOP, NULL},
    {"\\#", 35, CMR10, BINARYOP, NULL},
    {"$", 36, CMR10, BINARYOP, NULL},
    {"\\$", 36, CMR10, BINARYOP, NULL},
    {"%", 37, CMR10, BINARYOP, NULL},
    {"\\%", 37, CMR10, BINARYOP, NULL},
    {"\\percent", 37, CMR10, BINARYOP, NULL},
    {"&", 38, CMR10, BINARYOP, NULL},
    {"\\&", 38, CMR10, BINARYOP, NULL},
    {"\'", 39, CMR10, BINARYOP, NULL},
    {"\\\'", 39, CMR10, BINARYOP, NULL},
    {"\\apostrophe", 39, CMR10, ORDINARY, NULL},
    {"(", 40, CMR10, OPENING, NULL},
    {"\\(", 40, CMR10, OPENING, NULL},
    {")", 41, CMR10, CLOSING, NULL},
    {"\\)", 41, CMR10, CLOSING, NULL},
    {"*", 42, CMR10, BINARYOP, NULL},
    {"+", 43, CMR10, BINARYOP, NULL},
    {"/", 47, CMR10, BINARYOP, NULL},
    {":", 58, CMR10, ORDINARY, NULL},
    {"\\colon", 58, CMR10, OPERATOR, NULL},
    {";", 59, CMR10, ORDINARY, NULL},
    {"\\semicolon", 59, CMR10, ORDINARY, NULL},
    {"=", 61, CMR10, RELATION, NULL},
    {"?", 63, CMR10, BINARYOP, NULL},
    {"@", 64, CMR10, BINARYOP, NULL},
    {"[", 91, CMR10, OPENING, NULL},
    {"\\[", 91, CMR10, OPENING, NULL},
    {"]", 93, CMR10, CLOSING, NULL},
    {"\\]", 93, CMR10, CLOSING, NULL},
    {"\\^", 94, CMR10, BINARYOP, NULL},
    {"\\~", 126, CMR10, OPERATOR, NULL},
    /* --- uppercase letters --- */
    {"A", 65, CMR10, VARIABLE, NULL},
    {"B", 66, CMR10, VARIABLE, NULL},
    {"C", 67, CMR10, VARIABLE, NULL},
    {"D", 68, CMR10, VARIABLE, NULL},
    {"E", 69, CMR10, VARIABLE, NULL},
    {"F", 70, CMR10, VARIABLE, NULL},
    {"G", 71, CMR10, VARIABLE, NULL},
    {"H", 72, CMR10, VARIABLE, NULL},
    {"I", 73, CMR10, VARIABLE, NULL},
    {"J", 74, CMR10, VARIABLE, NULL},
    {"K", 75, CMR10, VARIABLE, NULL},
    {"L", 76, CMR10, VARIABLE, NULL},
    {"M", 77, CMR10, VARIABLE, NULL},
    {"N", 78, CMR10, VARIABLE, NULL},
    {"O", 79, CMR10, VARIABLE, NULL},
    {"P", 80, CMR10, VARIABLE, NULL},
    {"Q", 81, CMR10, VARIABLE, NULL},
    {"R", 82, CMR10, VARIABLE, NULL},
    {"S", 83, CMR10, VARIABLE, NULL},
    {"T", 84, CMR10, VARIABLE, NULL},
    {"U", 85, CMR10, VARIABLE, NULL},
    {"V", 86, CMR10, VARIABLE, NULL},
    {"W", 87, CMR10, VARIABLE, NULL},
    {"X", 88, CMR10, VARIABLE, NULL},
    {"Y", 89, CMR10, VARIABLE, NULL},
    {"Z", 90, CMR10, VARIABLE, NULL},
    /* --- lowercase letters --- */
    {"a", 97, CMR10, VARIABLE, NULL},
    {"b", 98, CMR10, VARIABLE, NULL},
    {"c", 99, CMR10, VARIABLE, NULL},
    {"d", 100, CMR10, VARIABLE, NULL},
    {"e", 101, CMR10, VARIABLE, NULL},
    {"f", 102, CMR10, VARIABLE, NULL},
    {"g", 103, CMR10, VARIABLE, NULL},
    {"h", 104, CMR10, VARIABLE, NULL},
    {"i", 105, CMR10, VARIABLE, NULL},
    {"j", 106, CMR10, VARIABLE, NULL},
    {"k", 107, CMR10, VARIABLE, NULL},
    {"l", 108, CMR10, VARIABLE, NULL},
    {"m", 109, CMR10, VARIABLE, NULL},
    {"n", 110, CMR10, VARIABLE, NULL},
    {"o", 111, CMR10, VARIABLE, NULL},
    {"p", 112, CMR10, VARIABLE, NULL},
    {"q", 113, CMR10, VARIABLE, NULL},
    {"r", 114, CMR10, VARIABLE, NULL},
    {"s", 115, CMR10, VARIABLE, NULL},
    {"t", 116, CMR10, VARIABLE, NULL},
    {"u", 117, CMR10, VARIABLE, NULL},
    {"v", 118, CMR10, VARIABLE, NULL},
    {"w", 119, CMR10, VARIABLE, NULL},
    {"x", 120, CMR10, VARIABLE, NULL},
    {"y", 121, CMR10, VARIABLE, NULL},
    {"z", 122, CMR10, VARIABLE, NULL},
    /* --------------------- C M E X --------------------------
       symbol     charnum    family    class     function
       -------------------------------------------------------- */
    /* --- parens ()'s --- */
    {"\\big(", 0, CMEX10, OPENING, NULL},
    {"\\big)", 1, CMEX10, CLOSING, NULL},
    {"\\Big(", 16, CMEX10, OPENING, NULL},
    {"\\Big)", 17, CMEX10, CLOSING, NULL},
    {"\\bigg(", 18, CMEX10, OPENING, NULL},
    {"\\bigg)", 19, CMEX10, CLOSING, NULL},
    {"\\Bigg(", 32, CMEX10, OPENING, NULL},
    {"\\Bigg)", 33, CMEX10, CLOSING, NULL},
    {"\\bigl(", 0, CMEX10, OPENING, NULL},
    {"\\bigr)", 1, CMEX10, CLOSING, NULL},
    {"\\Bigl(", 16, CMEX10, OPENING, NULL},
    {"\\Bigr)", 17, CMEX10, CLOSING, NULL},
    {"\\biggl(", 18, CMEX10, OPENING, NULL},
    {"\\biggr)", 19, CMEX10, CLOSING, NULL},
    {"\\Biggl(", 32, CMEX10, OPENING, NULL},
    {"\\Biggr)", 33, CMEX10, CLOSING, NULL},
    /* --- brackets []'s --- */
    {"\\big[", 2, CMEX10, OPENING, NULL},
    {"\\big]", 3, CMEX10, CLOSING, NULL},
    {"\\bigg[", 20, CMEX10, OPENING, NULL},
    {"\\bigg]", 21, CMEX10, CLOSING, NULL},
    {"\\Bigg[", 34, CMEX10, OPENING, NULL},
    {"\\Bigg]", 35, CMEX10, CLOSING, NULL},
    {"\\Big[", 104, CMEX10, OPENING, NULL},
    {"\\Big]", 105, CMEX10, CLOSING, NULL},
    {"\\bigl[", 2, CMEX10, OPENING, NULL},
    {"\\bigr]", 3, CMEX10, CLOSING, NULL},
    {"\\biggl[", 20, CMEX10, OPENING, NULL},
    {"\\biggr]", 21, CMEX10, CLOSING, NULL},
    {"\\Biggl[", 34, CMEX10, OPENING, NULL},
    {"\\Biggr]", 35, CMEX10, CLOSING, NULL},
    {"\\Bigl[", 104, CMEX10, OPENING, NULL},
    {"\\Bigr]", 105, CMEX10, CLOSING, NULL},
    /* --- braces {}'s --- */
    {"\\big{", 8, CMEX10, OPENING, NULL},
    {"\\big}", 9, CMEX10, CLOSING, NULL},
    {"\\bigg{", 26, CMEX10, OPENING, NULL},
    {"\\bigg}", 27, CMEX10, CLOSING, NULL},
    {"\\Bigg{", 40, CMEX10, OPENING, NULL},
    {"\\Bigg}", 41, CMEX10, CLOSING, NULL},
    {"\\Big{", 110, CMEX10, OPENING, NULL},
    {"\\Big}", 111, CMEX10, CLOSING, NULL},
    {"\\bigl{", 8, CMEX10, OPENING, NULL},
    {"\\bigr}", 9, CMEX10, CLOSING, NULL},
    {"\\biggl{", 26, CMEX10, OPENING, NULL},
    {"\\biggr}", 27, CMEX10, CLOSING, NULL},
    {"\\Biggl{", 40, CMEX10, OPENING, NULL},
    {"\\Biggr}", 41, CMEX10, CLOSING, NULL},
    {"\\Bigl{", 110, CMEX10, OPENING, NULL},
    {"\\Bigr}", 111, CMEX10, CLOSING, NULL},
    {"\\big\\{", 8, CMEX10, OPENING, NULL},
    {"\\big\\}", 9, CMEX10, CLOSING, NULL},
    {"\\bigg\\{", 26, CMEX10, OPENING, NULL},
    {"\\bigg\\}", 27, CMEX10, CLOSING, NULL},
    {"\\Bigg\\{", 40, CMEX10, OPENING, NULL},
    {"\\Bigg\\}", 41, CMEX10, CLOSING, NULL},
    {"\\Big\\{", 110, CMEX10, OPENING, NULL},
    {"\\Big\\}", 111, CMEX10, CLOSING, NULL},
    {"\\bigl\\{", 8, CMEX10, OPENING, NULL},
    {"\\bigr\\}", 9, CMEX10, CLOSING, NULL},
    {"\\biggl\\{", 26, CMEX10, OPENING, NULL},
    {"\\biggr\\}", 27, CMEX10, CLOSING, NULL},
    {"\\Biggl\\{", 40, CMEX10, OPENING, NULL},
    {"\\Biggr\\}", 41, CMEX10, CLOSING, NULL},
    {"\\Bigl\\{", 110, CMEX10, OPENING, NULL},
    {"\\Bigr\\}", 111, CMEX10, CLOSING, NULL},
    {"\\big\\lbrace", 8, CMEX10, OPENING, NULL},
    {"\\big\\rbrace", 9, CMEX10, CLOSING, NULL},
    {"\\bigg\\lbrace", 26, CMEX10, OPENING, NULL},
    {"\\bigg\\rbrace", 27, CMEX10, CLOSING, NULL},
    {"\\Bigg\\lbrace", 40, CMEX10, OPENING, NULL},
    {"\\Bigg\\rbrace", 41, CMEX10, CLOSING, NULL},
    {"\\Big\\lbrace", 110, CMEX10, OPENING, NULL},
    {"\\Big\\rbrace", 111, CMEX10, CLOSING, NULL},
    /* --- angles <>'s --- */
    {"\\big<", 10, CMEX10, OPENING, NULL},
    {"\\big>", 11, CMEX10, CLOSING, NULL},
    {"\\bigg<", 28, CMEX10, OPENING, NULL},
    {"\\bigg>", 29, CMEX10, CLOSING, NULL},
    {"\\Bigg<", 42, CMEX10, OPENING, NULL},
    {"\\Bigg>", 43, CMEX10, CLOSING, NULL},
    {"\\Big<", 68, CMEX10, OPENING, NULL},
    {"\\Big>", 69, CMEX10, CLOSING, NULL},
    {"\\bigl<", 10, CMEX10, OPENING, NULL},
    {"\\bigr>", 11, CMEX10, CLOSING, NULL},
    {"\\biggl<", 28, CMEX10, OPENING, NULL},
    {"\\biggr>", 29, CMEX10, CLOSING, NULL},
    {"\\Biggl<", 42, CMEX10, OPENING, NULL},
    {"\\Biggr>", 43, CMEX10, CLOSING, NULL},
    {"\\Bigl<", 68, CMEX10, OPENING, NULL},
    {"\\Bigr>", 69, CMEX10, CLOSING, NULL},
    {"\\big\\langle", 10, CMEX10, OPENING, NULL},
    {"\\big\\rangle", 11, CMEX10, CLOSING, NULL},
    {"\\bigg\\langle", 28, CMEX10, OPENING, NULL},
    {"\\bigg\\rangle", 29, CMEX10, CLOSING, NULL},
    {"\\Bigg\\langle", 42, CMEX10, OPENING, NULL},
    {"\\Bigg\\rangle", 43, CMEX10, CLOSING, NULL},
    {"\\Big\\langle", 68, CMEX10, OPENING, NULL},
    {"\\Big\\rangle", 69, CMEX10, CLOSING, NULL},
    /* --- hats ^ --- */
    {"^", 98, CMEX10, OPERATOR, NULL},
    {"^", 99, CMEX10, OPERATOR, NULL},
    {"^", 100, CMEX10, OPERATOR, NULL},
    /* --- tildes --- */
    {"~", 101, CMEX10, OPERATOR, NULL},
    {"~", 102, CMEX10, OPERATOR, NULL},
    {"~", 103, CMEX10, OPERATOR, NULL},
    /* --- /'s --- */
    {"/", 44, CMEX10, OPENING, NULL},
    {"/", 46, CMEX10, OPENING, NULL},
    {"\\", 45, CMEX10, OPENING, NULL},
    {"\\", 47, CMEX10, OPENING, NULL},
    /* --- \sum, \int and other (displaymath) symbols --- */
    {"\\bigsqcup", 70, CMEX10, LOWERBIG, NULL},
    {"\\Bigsqcup", 71, CMEX10, UPPERBIG, NULL},
    {"\\oint", 72, CMEX10, OPERATOR, NULL},
    {"\\bigoint", 72, CMEX10, LOWERBIG, NULL},
    {"\\Bigoint", 73, CMEX10, UPPERBIG, NULL},
    {"\\bigodot", 74, CMEX10, LOWERBIG, NULL},
    {"\\Bigodot", 75, CMEX10, UPPERBIG, NULL},
    {"\\bigoplus", 76, CMEX10, LOWERBIG, NULL},
    {"\\Bigoplus", 77, CMEX10, UPPERBIG, NULL},
    {"\\bigotimes", 78, CMEX10, LOWERBIG, NULL},
    {"\\Bigotimes", 79, CMEX10, UPPERBIG, NULL},
    {"\\sum", 80, CMEX10, OPERATOR, NULL},
    {"\\bigsum", 80, CMEX10, LOWERBIG, NULL},
    {"\\prod", 81, CMEX10, OPERATOR, NULL},
    {"\\bigprod", 81, CMEX10, LOWERBIG, NULL},
    {"\\int", 82, CMEX10, OPERATOR, NULL},
    {"\\bigint", 82, CMEX10, LOWERBIG, NULL},
    {"\\bigcup", 83, CMEX10, LOWERBIG, NULL},
    {"\\bigcap", 84, CMEX10, LOWERBIG, NULL},
    {"\\biguplus", 85, CMEX10, LOWERBIG, NULL},
    {"\\bigwedge", 86, CMEX10, LOWERBIG, NULL},
    {"\\bigvee", 87, CMEX10, LOWERBIG, NULL},
    {"\\Bigsum", 88, CMEX10, UPPERBIG, NULL},
    {"\\big\\sum", 88, CMEX10, UPPERBIG, NULL},
    {"\\Big\\sum", 88, CMEX10, UPPERBIG, NULL},
    {"\\bigg\\sum", 88, CMEX10, UPPERBIG, NULL},
    {"\\Bigg\\sum", 88, CMEX10, UPPERBIG, NULL},
    {"\\Bigprod", 89, CMEX10, UPPERBIG, NULL},
    {"\\Bigint", 90, CMEX10, UPPERBIG, NULL},
    {"\\big\\int", 90, CMEX10, UPPERBIG, NULL},
    {"\\Big\\int", 90, CMEX10, UPPERBIG, NULL},
    {"\\bigg\\int", 90, CMEX10, UPPERBIG, NULL},
    {"\\Bigg\\int", 90, CMEX10, UPPERBIG, NULL},
    {"\\Bigcup", 91, CMEX10, UPPERBIG, NULL},
    {"\\Bigcap", 92, CMEX10, UPPERBIG, NULL},
    {"\\Biguplus", 93, CMEX10, UPPERBIG, NULL},
    {"\\Bigwedge", 94, CMEX10, UPPERBIG, NULL},
    {"\\Bigvee", 95, CMEX10, UPPERBIG, NULL},
    {"\\coprod", 96, CMEX10, LOWERBIG, NULL},
    {"\\bigcoprod", 96, CMEX10, LOWERBIG, NULL},
    {"\\Bigcoprod", 97, CMEX10, UPPERBIG, NULL},
    /* --- symbol pieces (see TeXbook page 432) --- */
    {"\\leftbracetop", 56, CMEX10, OPENING, NULL},
    {"\\rightbracetop", 57, CMEX10, CLOSING, NULL},
    {"\\leftbracebot", 58, CMEX10, OPENING, NULL},
    {"\\rightbracebot", 59, CMEX10, CLOSING, NULL},
    {"\\leftbracemid", 60, CMEX10, OPENING, NULL},
    {"\\rightbracemid", 61, CMEX10, CLOSING, NULL},
    {"\\leftbracebar", 62, CMEX10, OPENING, NULL},
    {"\\rightbracebar", 62, CMEX10, CLOSING, NULL},
    {"\\leftparentop", 48, CMEX10, OPENING, NULL},
    {"\\rightparentop", 49, CMEX10, CLOSING, NULL},
    {"\\leftparenbot", 64, CMEX10, OPENING, NULL},
    {"\\rightparenbot", 65, CMEX10, CLOSING, NULL},
    {"\\leftparenbar", 66, CMEX10, OPENING, NULL},
    {"\\rightparenbar", 67, CMEX10, CLOSING, NULL},
    /* --------------------- R S F S --------------------------
       symbol     charnum    family    class     function
       -------------------------------------------------------- */
    /* --- rsfs script letters (written as \scr{A...Z}) --- */
    {"A", 0, RSFS10, VARIABLE, NULL},
    {"B", 1, RSFS10, VARIABLE, NULL},
    {"C", 2, RSFS10, VARIABLE, NULL},
    {"D", 3, RSFS10, VARIABLE, NULL},
    {"E", 4, RSFS10, VARIABLE, NULL},
    {"F", 5, RSFS10, VARIABLE, NULL},
    {"G", 6, RSFS10, VARIABLE, NULL},
    {"H", 7, RSFS10, VARIABLE, NULL},
    {"I", 8, RSFS10, VARIABLE, NULL},
    {"J", 9, RSFS10, VARIABLE, NULL},
    {"K", 10, RSFS10, VARIABLE, NULL},
    {"L", 11, RSFS10, VARIABLE, NULL},
    {"M", 12, RSFS10, VARIABLE, NULL},
    {"N", 13, RSFS10, VARIABLE, NULL},
    {"O", 14, RSFS10, VARIABLE, NULL},
    {"P", 15, RSFS10, VARIABLE, NULL},
    {"Q", 16, RSFS10, VARIABLE, NULL},
    {"R", 17, RSFS10, VARIABLE, NULL},
    {"S", 18, RSFS10, VARIABLE, NULL},
    {"T", 19, RSFS10, VARIABLE, NULL},
    {"U", 20, RSFS10, VARIABLE, NULL},
    {"V", 21, RSFS10, VARIABLE, NULL},
    {"W", 22, RSFS10, VARIABLE, NULL},
    {"X", 23, RSFS10, VARIABLE, NULL},
    {"Y", 24, RSFS10, VARIABLE, NULL},
    {"Z", 25, RSFS10, VARIABLE, NULL},
    /* --- rsfs script letters (written as \scrA...\scrZ) --- */
    {"\\scrA", 0, RSFS10, VARIABLE, NULL},
    {"\\scrB", 1, RSFS10, VARIABLE, NULL},
    {"\\scrC", 2, RSFS10, VARIABLE, NULL},
    {"\\scrD", 3, RSFS10, VARIABLE, NULL},
    {"\\scrE", 4, RSFS10, VARIABLE, NULL},
    {"\\scrF", 5, RSFS10, VARIABLE, NULL},
    {"\\scrG", 6, RSFS10, VARIABLE, NULL},
    {"\\scrH", 7, RSFS10, VARIABLE, NULL},
    {"\\scrI", 8, RSFS10, VARIABLE, NULL},
    {"\\scrJ", 9, RSFS10, VARIABLE, NULL},
    {"\\scrK", 10, RSFS10, VARIABLE, NULL},
    {"\\scrL", 11, RSFS10, VARIABLE, NULL},
    {"\\scrM", 12, RSFS10, VARIABLE, NULL},
    {"\\scrN", 13, RSFS10, VARIABLE, NULL},
    {"\\scrO", 14, RSFS10, VARIABLE, NULL},
    {"\\scrP", 15, RSFS10, VARIABLE, NULL},
    {"\\scrQ", 16, RSFS10, VARIABLE, NULL},
    {"\\scrR", 17, RSFS10, VARIABLE, NULL},
    {"\\scrS", 18, RSFS10, VARIABLE, NULL},
    {"\\scrT", 19, RSFS10, VARIABLE, NULL},
    {"\\scrU", 20, RSFS10, VARIABLE, NULL},
    {"\\scrV", 21, RSFS10, VARIABLE, NULL},
    {"\\scrW", 22, RSFS10, VARIABLE, NULL},
    {"\\scrX", 23, RSFS10, VARIABLE, NULL},
    {"\\scrY", 24, RSFS10, VARIABLE, NULL},
    {"\\scrZ", 25, RSFS10, VARIABLE, NULL},
    /* -------------------- B B O L D -------------------------
       symbol     charnum    family    class     function
       -------------------------------------------------------- */
    /* --- uppercase greek letters --- */
    {"\\Gamma", 0, BBOLD10, VARIABLE, NULL},
    {"\\Delta", 1, BBOLD10, VARIABLE, NULL},
    {"\\Theta", 2, BBOLD10, VARIABLE, NULL},
    {"\\Lambda", 3, BBOLD10, VARIABLE, NULL},
    {"\\Xi", 4, BBOLD10, VARIABLE, NULL},
    {"\\Pi", 5, BBOLD10, VARIABLE, NULL},
    {"\\Sigma", 6, BBOLD10, VARIABLE, NULL},
    {"\\smallsum", 6, BBOLD10, OPERATOR, NULL},
    {"\\Upsilon", 7, BBOLD10, VARIABLE, NULL},
    {"\\Phi", 8, BBOLD10, VARIABLE, NULL},
    {"\\Psi", 9, BBOLD10, VARIABLE, NULL},
    {"\\Omega", 10, BBOLD10, VARIABLE, NULL},
    /* --- lowercase greek letters --- */
    {"\\alpha", 11, BBOLD10, VARIABLE, NULL},
    {"\\beta", 12, BBOLD10, VARIABLE, NULL},
    {"\\gamma", 13, BBOLD10, VARIABLE, NULL},
    {"\\delta", 14, BBOLD10, VARIABLE, NULL},
    {"\\epsilon", 15, BBOLD10, VARIABLE, NULL},
    {"\\zeta", 16, BBOLD10, VARIABLE, NULL},
    {"\\eta", 17, BBOLD10, VARIABLE, NULL},
    {"\\theta", 18, BBOLD10, VARIABLE, NULL},
    {"\\iota", 19, BBOLD10, VARIABLE, NULL},
    {"\\kappa", 20, BBOLD10, VARIABLE, NULL},
    {"\\lambda", 21, BBOLD10, VARIABLE, NULL},
    {"\\mu", 22, BBOLD10, VARIABLE, NULL},
    {"\\nu", 23, BBOLD10, VARIABLE, NULL},
    {"\\xi", 24, BBOLD10, VARIABLE, NULL},
    {"\\pi", 25, BBOLD10, VARIABLE, NULL},
    {"\\rho", 26, BBOLD10, VARIABLE, NULL},
    {"\\sigma", 27, BBOLD10, VARIABLE, NULL},
    {"\\tau", 28, BBOLD10, VARIABLE, NULL},
    {"\\upsilon", 29, BBOLD10, VARIABLE, NULL},
    {"\\phi", 30, BBOLD10, VARIABLE, NULL},
    {"\\chi", 31, BBOLD10, VARIABLE, NULL},
    {"\\psi", 32, BBOLD10, VARIABLE, NULL},
    {"\\omega", 127, BBOLD10, VARIABLE, NULL},
    /* --- digits 0-9 --- */
    {"0", 48, BBOLD10, ORDINARY, NULL},
    {"1", 49, BBOLD10, ORDINARY, NULL},
    {"2", 50, BBOLD10, ORDINARY, NULL},
    {"3", 51, BBOLD10, ORDINARY, NULL},
    {"4", 52, BBOLD10, ORDINARY, NULL},
    {"5", 53, BBOLD10, ORDINARY, NULL},
    {"6", 54, BBOLD10, ORDINARY, NULL},
    {"7", 55, BBOLD10, ORDINARY, NULL},
    {"8", 56, BBOLD10, ORDINARY, NULL},
    {"9", 57, BBOLD10, ORDINARY, NULL},
    {"\\0", 48, BBOLD10, ORDINARY, NULL},
    {"\\1", 49, BBOLD10, ORDINARY, NULL},
    {"\\2", 50, BBOLD10, ORDINARY, NULL},
    {"\\3", 51, BBOLD10, ORDINARY, NULL},
    {"\\4", 52, BBOLD10, ORDINARY, NULL},
    {"\\5", 53, BBOLD10, ORDINARY, NULL},
    {"\\6", 54, BBOLD10, ORDINARY, NULL},
    {"\\7", 55, BBOLD10, ORDINARY, NULL},
    {"\\8", 56, BBOLD10, ORDINARY, NULL},
    {"\\9", 57, BBOLD10, ORDINARY, NULL},
    /* --- uppercase letters --- */
    {"A", 65, BBOLD10, VARIABLE, NULL},
    {"B", 66, BBOLD10, VARIABLE, NULL},
    {"C", 67, BBOLD10, VARIABLE, NULL},
    {"D", 68, BBOLD10, VARIABLE, NULL},
    {"E", 69, BBOLD10, VARIABLE, NULL},
    {"F", 70, BBOLD10, VARIABLE, NULL},
    {"G", 71, BBOLD10, VARIABLE, NULL},
    {"H", 72, BBOLD10, VARIABLE, NULL},
    {"I", 73, BBOLD10, VARIABLE, NULL},
    {"J", 74, BBOLD10, VARIABLE, NULL},
    {"K", 75, BBOLD10, VARIABLE, NULL},
    {"L", 76, BBOLD10, VARIABLE, NULL},
    {"M", 77, BBOLD10, VARIABLE, NULL},
    {"N", 78, BBOLD10, VARIABLE, NULL},
    {"O", 79, BBOLD10, VARIABLE, NULL},
    {"P", 80, BBOLD10, VARIABLE, NULL},
    {"Q", 81, BBOLD10, VARIABLE, NULL},
    {"R", 82, BBOLD10, VARIABLE, NULL},
    {"S", 83, BBOLD10, VARIABLE, NULL},
    {"T", 84, BBOLD10, VARIABLE, NULL},
    {"U", 85, BBOLD10, VARIABLE, NULL},
    {"V", 86, BBOLD10, VARIABLE, NULL},
    {"W", 87, BBOLD10, VARIABLE, NULL},
    {"X", 88, BBOLD10, VARIABLE, NULL},
    {"Y", 89, BBOLD10, VARIABLE, NULL},
    {"Z", 90, BBOLD10, VARIABLE, NULL},
    /* --- lowercase letters --- */
    {"a", 97, BBOLD10, VARIABLE, NULL},
    {"b", 98, BBOLD10, VARIABLE, NULL},
    {"c", 99, BBOLD10, VARIABLE, NULL},
    {"d", 100, BBOLD10, VARIABLE, NULL},
    {"e", 101, BBOLD10, VARIABLE, NULL},
    {"f", 102, BBOLD10, VARIABLE, NULL},
    {"g", 103, BBOLD10, VARIABLE, NULL},
    {"h", 104, BBOLD10, VARIABLE, NULL},
    {"i", 105, BBOLD10, VARIABLE, NULL},
    {"j", 106, BBOLD10, VARIABLE, NULL},
    {"k", 107, BBOLD10, VARIABLE, NULL},
    {"l", 108, BBOLD10, VARIABLE, NULL},
    {"m", 109, BBOLD10, VARIABLE, NULL},
    {"n", 110, BBOLD10, VARIABLE, NULL},
    {"o", 111, BBOLD10, VARIABLE, NULL},
    {"p", 112, BBOLD10, VARIABLE, NULL},
    {"q", 113, BBOLD10, VARIABLE, NULL},
    {"r", 114, BBOLD10, VARIABLE, NULL},
    {"s", 115, BBOLD10, VARIABLE, NULL},
    {"t", 116, BBOLD10, VARIABLE, NULL},
    {"u", 117, BBOLD10, VARIABLE, NULL},
    {"v", 118, BBOLD10, VARIABLE, NULL},
    {"w", 119, BBOLD10, VARIABLE, NULL},
    {"x", 120, BBOLD10, VARIABLE, NULL},
    {"y", 121, BBOLD10, VARIABLE, NULL},
    {"z", 122, BBOLD10, VARIABLE, NULL},
    /* --- symbols, relations, etc --- */
    {"!", 33, BBOLD10, BINARYOP, NULL},
    {"#", 35, BBOLD10, BINARYOP, NULL},
    {"\\#", 35, BBOLD10, BINARYOP, NULL},
    {"$", 36, BBOLD10, BINARYOP, NULL},
    {"\\$", 36, BBOLD10, BINARYOP, NULL},
    {"%", 37, BBOLD10, BINARYOP, NULL},
    {"\\%", 37, BBOLD10, BINARYOP, NULL},
    {"\\percent", 37, BBOLD10, BINARYOP, NULL},
    {"&", 38, BBOLD10, BINARYOP, NULL},
    {"\\&", 38, BBOLD10, BINARYOP, NULL},
    {"\'", 39, BBOLD10, BINARYOP, NULL},
    {"\\apostrophe", 39, BBOLD10, ORDINARY, NULL},
    {"(", 40, BBOLD10, OPENING, NULL},
    {"\\(", 40, BBOLD10, OPENING, NULL},
    {")", 41, BBOLD10, CLOSING, NULL},
    {"\\)", 41, BBOLD10, CLOSING, NULL},
    {"*", 42, BBOLD10, BINARYOP, NULL},
    {"+", 43, BBOLD10, BINARYOP, NULL},
    {",", 44, BBOLD10, PUNCTION, NULL},
    {"-", 45, BBOLD10, BINARYOP, NULL},
    {".", 46, BBOLD10, PUNCTION, NULL},
    {"/", 47, BBOLD10, BINARYOP, NULL},
    {":", 58, BBOLD10, ORDINARY, NULL},
    {";", 59, BBOLD10, ORDINARY, NULL},
    {"<", 60, BBOLD10, RELATION, NULL},
    {"\\<", 60, BBOLD10, RELATION, NULL},
    {"\\cdot", 61, BBOLD10, BINARYOP, NULL},
    {">", 62, BBOLD10, RELATION, NULL},
    {"\\>", 62, BBOLD10, RELATION, NULL},
    {"?", 63, BBOLD10, BINARYOP, NULL},
    {"@", 64, BBOLD10, BINARYOP, NULL},
    {"[", 91, BBOLD10, OPENING, NULL},
    {"\\[", 91, BBOLD10, OPENING, NULL},
    {"\\\\", 92, BBOLD10, OPENING, NULL},
    {"\\backslash", 92, BBOLD10, OPENING, NULL},
    {"]", 93, BBOLD10, CLOSING, NULL},
    {"\\]", 93, BBOLD10, CLOSING, NULL},
    {"|", 124, BBOLD10, BINARYOP, NULL},
    {"\\-", 123, BBOLD10, BINARYOP, NULL},
    /* ------------------- S T M A R Y ------------------------
       symbol     charnum    family    class     function
       -------------------------------------------------------- */
    /* --- stmaryrd symbols (see stmaryrd.sty for defs) --- */
    {"\\shortleftarrow", 0, STMARY10, ARROW, NULL},
    {"\\shortrightarrow", 1, STMARY10, ARROW, NULL},
    {"\\shortuparrow", 2, STMARY10, ARROW, NULL},
    {"\\shortdownarrow", 3, STMARY10, ARROW, NULL},
    {"\\Yup", 4, STMARY10, BINARYOP, NULL},
    {"\\Ydown", 5, STMARY10, BINARYOP, NULL},
    {"\\Yleft", 6, STMARY10, BINARYOP, NULL},
    {"\\Yright", 7, STMARY10, BINARYOP, NULL},
    {"\\varcurlyvee", 8, STMARY10, BINARYOP, NULL},
    {"\\varcurlywedge", 9, STMARY10, BINARYOP, NULL},
    {"\\minuso", 10, STMARY10, BINARYOP, NULL},
    {"\\baro", 11, STMARY10, BINARYOP, NULL},
    {"\\sslash", 12, STMARY10, BINARYOP, NULL},
    {"\\bblash", 13, STMARY10, BINARYOP, NULL},
    {"\\moo", 14, STMARY10, BINARYOP, NULL},
    {"\\varotimes", 15, STMARY10, BINARYOP, NULL},
    {"\\varoast", 16, STMARY10, BINARYOP, NULL},
    {"\\varobar", 17, STMARY10, BINARYOP, NULL},
    {"\\varodot", 18, STMARY10, BINARYOP, NULL},
    {"\\varoslash", 19, STMARY10, BINARYOP, NULL},
    {"\\varobslash", 20, STMARY10, BINARYOP, NULL},
    {"\\varocircle", 21, STMARY10, BINARYOP, NULL},
    {"\\varoplus", 22, STMARY10, BINARYOP, NULL},
    {"\\varominus", 23, STMARY10, BINARYOP, NULL},
    {"\\boxast", 24, STMARY10, BINARYOP, NULL},
    {"\\boxbar", 25, STMARY10, BINARYOP, NULL},
    {"\\boxdot", 26, STMARY10, BINARYOP, NULL},
    {"\\boxslash", 27, STMARY10, BINARYOP, NULL},
    {"\\boxbslash", 28, STMARY10, BINARYOP, NULL},
    {"\\boxcircle", 29, STMARY10, BINARYOP, NULL},
    {"\\boxbox", 30, STMARY10, BINARYOP, NULL},
    {"\\boxempty", 31, STMARY10, BINARYOP, NULL},
    {"\\qed", 31, STMARY10, BINARYOP, NULL},
    {"\\lightning", 32, STMARY10, ORDINARY, NULL},
    {"\\merge", 33, STMARY10, BINARYOP, NULL},
    {"\\vartimes", 34, STMARY10, BINARYOP, NULL},
    {"\\fatsemi", 35, STMARY10, BINARYOP, NULL},
    {"\\sswarrow", 36, STMARY10, ARROW, NULL},
    {"\\ssearrow", 37, STMARY10, ARROW, NULL},
    {"\\curlywedgeuparrow", 38, STMARY10, ARROW, NULL},
    {"\\curlywedgedownarrow", 39, STMARY10, ARROW, NULL},
    {"\\fatslash", 40, STMARY10, BINARYOP, NULL},
    {"\\fatbslash", 41, STMARY10, BINARYOP, NULL},
    {"\\lbag", 42, STMARY10, BINARYOP, NULL},
    {"\\rbag", 43, STMARY10, BINARYOP, NULL},
    {"\\varbigcirc", 44, STMARY10, BINARYOP, NULL},
    {"\\leftrightarroweq", 45, STMARY10, ARROW, NULL},
    {"\\curlyveedownarrow", 46, STMARY10, ARROW, NULL},
    {"\\curlyveeuparrow", 47, STMARY10, ARROW, NULL},
    {"\\nnwarrow", 48, STMARY10, ARROW, NULL},
    {"\\nnearrow", 49, STMARY10, ARROW, NULL},
    {"\\leftslice", 50, STMARY10, BINARYOP, NULL},
    {"\\rightslice", 51, STMARY10, BINARYOP, NULL},
    {"\\varolessthan", 52, STMARY10, BINARYOP, NULL},
    {"\\varogreaterthan", 53, STMARY10, BINARYOP, NULL},
    {"\\varovee", 54, STMARY10, BINARYOP, NULL},
    {"\\varowedge", 55, STMARY10, BINARYOP, NULL},
    {"\\talloblong", 56, STMARY10, BINARYOP, NULL},
    {"\\interleave", 57, STMARY10, BINARYOP, NULL},
    {"\\obar", 58, STMARY10, BINARYOP, NULL},
    {"\\oslash", 59, STMARY10, BINARYOP, NULL},
    {"\\olessthan", 60, STMARY10, BINARYOP, NULL},
    {"\\ogreaterthan", 61, STMARY10, BINARYOP, NULL},
    {"\\ovee", 62, STMARY10, BINARYOP, NULL},
    {"\\owedge", 63, STMARY10, BINARYOP, NULL},
    {"\\oblong", 64, STMARY10, BINARYOP, NULL},
    {"\\inplus", 65, STMARY10, RELATION, NULL},
    {"\\niplus", 66, STMARY10, RELATION, NULL},
    {"\\nplus", 67, STMARY10, BINARYOP, NULL},
    {"\\subsetplus", 68, STMARY10, RELATION, NULL},
    {"\\supsetplus", 69, STMARY10, RELATION, NULL},
    {"\\subsetpluseq", 70, STMARY10, RELATION, NULL},
    {"\\supsetpluseq", 71, STMARY10, RELATION, NULL},
    {"\\Lbag", 72, STMARY10, OPENING, NULL},
    {"\\Rbag", 73, STMARY10, CLOSING, NULL},
    {"\\llbracket", 74, STMARY10, OPENING, NULL},
    {"\\rrbracket", 75, STMARY10, CLOSING, NULL},
    {"\\llparenthesis", 76, STMARY10, OPENING, NULL},
    {"\\rrparenthesis", 77, STMARY10, CLOSING, NULL},
    {"\\binampersand", 78, STMARY10, OPENING, NULL},
    {"\\bindnasrepma", 79, STMARY10, CLOSING, NULL},
    {"\\trianglelefteqslant", 80, STMARY10, RELATION, NULL},
    {"\\trianglerighteqslant", 81, STMARY10, RELATION, NULL},
    {"\\ntrianglelefteqslant", 82, STMARY10, RELATION, NULL},
    {"\\ntrianglerighteqslant", 83, STMARY10, RELATION, NULL},
    {"\\llfloor", 84, STMARY10, OPENING, NULL},
    {"\\rrfloor", 85, STMARY10, CLOSING, NULL},
    {"\\llceil", 86, STMARY10, OPENING, NULL},
    {"\\rrceil", 87, STMARY10, CLOSING, NULL},
    {"\\arrownot", 88, STMARY10, RELATION, NULL},
    {"\\Arrownot", 89, STMARY10, RELATION, NULL},
    {"\\Mapstochar", 90, STMARY10, RELATION, NULL},
    {"\\mapsfromchar", 91, STMARY10, RELATION, NULL},
    {"\\Mapsfromchar", 92, STMARY10, RELATION, NULL},
    {"\\leftrightarrowtriangle", 93, STMARY10, BINARYOP, NULL},
    {"\\leftarrowtriangle", 94, STMARY10, RELATION, NULL},
    {"\\rightarrowtriangle", 95, STMARY10, RELATION, NULL},
    {"\\bigtriangledown", 96, STMARY10, OPERATOR, NULL},
    {"\\bigtriangleup", 97, STMARY10, OPERATOR, NULL},
    {"\\bigcurlyvee", 98, STMARY10, OPERATOR, NULL},
    {"\\bigcurlywedge", 99, STMARY10, OPERATOR, NULL},
    {"\\bigsqcap", 100, STMARY10, OPERATOR, NULL},
    {"\\Bigsqcap", 100, STMARY10, OPERATOR, NULL},
    {"\\bigbox", 101, STMARY10, OPERATOR, NULL},
    {"\\bigparallel", 102, STMARY10, OPERATOR, NULL},
    {"\\biginterleave", 103, STMARY10, OPERATOR, NULL},
    {"\\bignplus", 112, STMARY10, OPERATOR, NULL},
    /* ---------------------- C Y R ---------------------------
       symbol     charnum    family    class     function
       -------------------------------------------------------- */
    /* ---
     * undefined: 20,21,28,29,33-59,61,63,64,91,92,93,96,123,124
     * ---------------------------------------------------------- */
    /* --- special characters --- */
    {"\\cyddot", 32, CYR10, VARIABLE, NULL},
    /* ---See amsfndoc.dvi Figure 1 Input Conventions for AMS cyrillic--- */
    {"A", 65, CYR10, VARIABLE, NULL},
    {"a", 97, CYR10, VARIABLE, NULL},
    {"B", 66, CYR10, VARIABLE, NULL},
    {"b", 98, CYR10, VARIABLE, NULL},
    {"V", 86, CYR10, VARIABLE, NULL},
    {"v", 118, CYR10, VARIABLE, NULL},
    {"G", 71, CYR10, VARIABLE, NULL},
    {"g", 103, CYR10, VARIABLE, NULL},
    {"D", 68, CYR10, VARIABLE, NULL},
    {"d", 100, CYR10, VARIABLE, NULL},
    {"Dj", 6, CYR10, VARIABLE, NULL},
    {"DJ", 6, CYR10, VARIABLE, NULL},
    {"dj", 14, CYR10, VARIABLE, NULL},
    {"E", 69, CYR10, VARIABLE, NULL},
    {"e", 101, CYR10, VARIABLE, NULL},
    {"\\\"E", 19, CYR10, VARIABLE, NULL},
    {"\\\"e", 27, CYR10, VARIABLE, NULL},
    {"\\=E", 5, CYR10, VARIABLE, NULL},
    {"\\=e", 13, CYR10, VARIABLE, NULL},
    {"Zh", 17, CYR10, VARIABLE, NULL},
    {"ZH", 17, CYR10, VARIABLE, NULL},
    {"zh", 25, CYR10, VARIABLE, NULL},
    {"Z", 90, CYR10, VARIABLE, NULL},
    {"z", 122, CYR10, VARIABLE, NULL},
    {"I", 73, CYR10, VARIABLE, NULL},
    {"i", 105, CYR10, VARIABLE, NULL},
    {"\\=I", 4, CYR10, VARIABLE, NULL},
    {"\\=\\i", 12, CYR10, VARIABLE, NULL},
    {"J", 74, CYR10, VARIABLE, NULL},
    {"j", 106, CYR10, VARIABLE, NULL},
    {"\\u I", 18, CYR10, VARIABLE, NULL},
    {"\\u\\i", 26, CYR10, VARIABLE, NULL},
    {"K", 75, CYR10, VARIABLE, NULL},
    {"k", 107, CYR10, VARIABLE, NULL},
    {"L", 76, CYR10, VARIABLE, NULL},
    {"l", 108, CYR10, VARIABLE, NULL},
    {"Lj", 1, CYR10, VARIABLE, NULL},
    {"LJ", 1, CYR10, VARIABLE, NULL},
    {"lj", 9, CYR10, VARIABLE, NULL},
    {"M", 77, CYR10, VARIABLE, NULL},
    {"m", 109, CYR10, VARIABLE, NULL},
    {"N", 78, CYR10, VARIABLE, NULL},
    {"n", 110, CYR10, VARIABLE, NULL},
    {"Nj", 0, CYR10, VARIABLE, NULL},
    {"NJ", 0, CYR10, VARIABLE, NULL},
    {"nj", 8, CYR10, VARIABLE, NULL},
    {"O", 79, CYR10, VARIABLE, NULL},
    {"o", 111, CYR10, VARIABLE, NULL},
    {"P", 80, CYR10, VARIABLE, NULL},
    {"p", 112, CYR10, VARIABLE, NULL},
    {"R", 82, CYR10, VARIABLE, NULL},
    {"r", 114, CYR10, VARIABLE, NULL},
    {"S", 83, CYR10, VARIABLE, NULL},
    {"s", 115, CYR10, VARIABLE, NULL},
    {"T", 84, CYR10, VARIABLE, NULL},
    {"t", 116, CYR10, VARIABLE, NULL},
    {"\\\'C", 7, CYR10, VARIABLE, NULL},
    {"\\\'c", 15, CYR10, VARIABLE, NULL},
    {"U", 85, CYR10, VARIABLE, NULL},
    {"u", 117, CYR10, VARIABLE, NULL},
    {"F", 70, CYR10, VARIABLE, NULL},
    {"f", 102, CYR10, VARIABLE, NULL},
    {"Kh", 72, CYR10, VARIABLE, NULL},
    {"KH", 72, CYR10, VARIABLE, NULL},
    {"kh", 104, CYR10, VARIABLE, NULL},
    {"Ts", 67, CYR10, VARIABLE, NULL},
    {"TS", 67, CYR10, VARIABLE, NULL},
    {"ts", 99, CYR10, VARIABLE, NULL},
    {"Ch", 81, CYR10, VARIABLE, NULL},
    {"CH", 81, CYR10, VARIABLE, NULL},
    {"ch", 113, CYR10, VARIABLE, NULL},
    {"Dzh", 2, CYR10, VARIABLE, NULL},
    {"DZH", 2, CYR10, VARIABLE, NULL},
    {"dzh", 10, CYR10, VARIABLE, NULL},
    {"Sh", 88, CYR10, VARIABLE, NULL},
    {"SH", 88, CYR10, VARIABLE, NULL},
    {"sh", 120, CYR10, VARIABLE, NULL},
    {"Shch", 87, CYR10, VARIABLE, NULL},
    {"SHCH", 87, CYR10, VARIABLE, NULL},
    {"shch", 119, CYR10, VARIABLE, NULL},
    {"\\Cdprime", 95, CYR10, VARIABLE, NULL},
    {"\\cdprime", 127, CYR10, VARIABLE, NULL},
    {"Y", 89, CYR10, VARIABLE, NULL},
    {"y", 121, CYR10, VARIABLE, NULL},
    {"\\Cprime", 94, CYR10, VARIABLE, NULL},
    {"\\cprime", 126, CYR10, VARIABLE, NULL},
    {"\\`E", 3, CYR10, VARIABLE, NULL},
    {"\\`e", 11, CYR10, VARIABLE, NULL},
    {"Yu", 16, CYR10, VARIABLE, NULL},
    {"YU", 16, CYR10, VARIABLE, NULL},
    {"yu", 24, CYR10, VARIABLE, NULL},
    {"Ya", 23, CYR10, VARIABLE, NULL},
    {"YA", 23, CYR10, VARIABLE, NULL},
    {"ya", 31, CYR10, VARIABLE, NULL},
    {"\\Dz", 22, CYR10, VARIABLE, NULL},
    {"\\dz", 30, CYR10, VARIABLE, NULL},
    {"N0", 125, CYR10, VARIABLE, NULL},
    {"<", 60, CYR10, VARIABLE, NULL},
    {">", 62, CYR10, VARIABLE, NULL},

    /* ------------------- C M M I G R ------------------------
       Using "Beta code" <http://en.wikipedia.org/wiki/Beta_code>
       to represent Greek characters in latin, e.g., type a to get
       \alpha, etc.
       symbol     charnum    family    class     function
       -------------------------------------------------------- */
    /* --- uppercase greek letters --- */
    {"G" /*\Gamma */ , 0, CMMI10GR, VARIABLE, NULL},
    {"D" /*\Delta */ , 1, CMMI10GR, VARIABLE, NULL},
    {"Q" /*\Theta */ , 2, CMMI10GR, VARIABLE, NULL},
    {"L" /*\Lambda */ , 3, CMMI10GR, VARIABLE, NULL},
    {"C" /*\Xi */ , 4, CMMI10GR, VARIABLE, NULL},
    {"P" /*\Pi */ , 5, CMMI10GR, VARIABLE, NULL},
    {"S" /*\Sigma */ , 6, CMMI10GR, VARIABLE, NULL},
    {"U" /*\Upsilon */ , 7, CMMI10GR, VARIABLE, NULL},
    {"F" /*\Phi */ , 8, CMMI10GR, VARIABLE, NULL},
    {"Y" /*\Psi */ , 9, CMMI10GR, VARIABLE, NULL},
    {"W" /*\Omega */ , 10, CMMI10GR, VARIABLE, NULL},
    /* --- lowercase greek letters --- */
    {"a" /*\alpha */ , 11, CMMI10GR, VARIABLE, NULL},
    {"b" /*\beta */ , 12, CMMI10GR, VARIABLE, NULL},
    {"g" /*\gamma */ , 13, CMMI10GR, VARIABLE, NULL},
    {"d" /*\delta */ , 14, CMMI10GR, VARIABLE, NULL},
    {"e" /*\epsilon */ , 15, CMMI10GR, VARIABLE, NULL},
    {"z" /*\zeta */ , 16, CMMI10GR, VARIABLE, NULL},
    {"h" /*\eta */ , 17, CMMI10GR, VARIABLE, NULL},
    {"q" /*\theta */ , 18, CMMI10GR, VARIABLE, NULL},
    {"i" /*\iota */ , 19, CMMI10GR, VARIABLE, NULL},
    {"k" /*\kappa */ , 20, CMMI10GR, VARIABLE, NULL},
    {"l" /*\lambda */ , 21, CMMI10GR, VARIABLE, NULL},
    {"m" /*\mu */ , 22, CMMI10GR, VARIABLE, NULL},
    {"n" /*\nu */ , 23, CMMI10GR, VARIABLE, NULL},
    {"c" /*\xi */ , 24, CMMI10GR, VARIABLE, NULL},
    {"p" /*\pi */ , 25, CMMI10GR, VARIABLE, NULL},
    {"r" /*\rho */ , 26, CMMI10GR, VARIABLE, NULL},
    {"s" /*\sigma */ , 27, CMMI10GR, VARIABLE, NULL},
    {"t" /*\tau */ , 28, CMMI10GR, VARIABLE, NULL},
    {"u" /*\upsilon */ , 29, CMMI10GR, VARIABLE, NULL},
    {"f" /*\phi */ , 30, CMMI10GR, VARIABLE, NULL},
    {"x" /*\chi */ , 31, CMMI10GR, VARIABLE, NULL},
    {"y" /*\psi */ , 32, CMMI10GR, VARIABLE, NULL},
    {"w" /*\omega */ , 33, CMMI10GR, VARIABLE, NULL},
#if 0
    {"?" /*\varepsilon */ , 34, CMMI10GR, VARIABLE, NULL},
    {"?" /*\vartheta */ , 35, CMMI10GR, VARIABLE, NULL},
    {"?" /*\varpi */ , 36, CMMI10GR, VARIABLE, NULL},
    {"?" /*\varrho */ , 37, CMMI10GR, VARIABLE, NULL},
    {"?" /*\varsigma */ , 38, CMMI10GR, VARIABLE, NULL},
    {"?" /*\varphi */ , 39, CMMI10GR, VARIABLE, NULL},
#endif
    /* ------------------- C M M I B G R ----------------------
       Using "Beta code" <http://en.wikipedia.org/wiki/Beta_code>
       to represent Greek characters in latin, e.g., type a to get
       \alpha, etc.
       symbol     charnum    family    class     function
       -------------------------------------------------------- */
    /* --- uppercase greek letters --- */
    {"G" /*\Gamma */ , 0, CMMI10BGR, VARIABLE, NULL},
    {"D" /*\Delta */ , 1, CMMI10BGR, VARIABLE, NULL},
    {"Q" /*\Theta */ , 2, CMMI10BGR, VARIABLE, NULL},
    {"L" /*\Lambda */ , 3, CMMI10BGR, VARIABLE, NULL},
    {"C" /*\Xi */ , 4, CMMI10BGR, VARIABLE, NULL},
    {"P" /*\Pi */ , 5, CMMI10BGR, VARIABLE, NULL},
    {"S" /*\Sigma */ , 6, CMMI10BGR, VARIABLE, NULL},
    {"U" /*\Upsilon */ , 7, CMMI10BGR, VARIABLE, NULL},
    {"F" /*\Phi */ , 8, CMMI10BGR, VARIABLE, NULL},
    {"Y" /*\Psi */ , 9, CMMI10BGR, VARIABLE, NULL},
    {"W" /*\Omega */ , 10, CMMI10BGR, VARIABLE, NULL},
    /* --- lowercase greek letters --- */
    {"a" /*\alpha */ , 11, CMMI10BGR, VARIABLE, NULL},
    {"b" /*\beta */ , 12, CMMI10BGR, VARIABLE, NULL},
    {"g" /*\gamma */ , 13, CMMI10BGR, VARIABLE, NULL},
    {"d" /*\delta */ , 14, CMMI10BGR, VARIABLE, NULL},
    {"e" /*\epsilon */ , 15, CMMI10BGR, VARIABLE, NULL},
    {"z" /*\zeta */ , 16, CMMI10BGR, VARIABLE, NULL},
    {"h" /*\eta */ , 17, CMMI10BGR, VARIABLE, NULL},
    {"q" /*\theta */ , 18, CMMI10BGR, VARIABLE, NULL},
    {"i" /*\iota */ , 19, CMMI10BGR, VARIABLE, NULL},
    {"k" /*\kappa */ , 20, CMMI10BGR, VARIABLE, NULL},
    {"l" /*\lambda */ , 21, CMMI10BGR, VARIABLE, NULL},
    {"m" /*\mu */ , 22, CMMI10BGR, VARIABLE, NULL},
    {"n" /*\nu */ , 23, CMMI10BGR, VARIABLE, NULL},
    {"c" /*\xi */ , 24, CMMI10BGR, VARIABLE, NULL},
    {"p" /*\pi */ , 25, CMMI10BGR, VARIABLE, NULL},
    {"r" /*\rho */ , 26, CMMI10BGR, VARIABLE, NULL},
    {"s" /*\sigma */ , 27, CMMI10BGR, VARIABLE, NULL},
    {"t" /*\tau */ , 28, CMMI10BGR, VARIABLE, NULL},
    {"u" /*\upsilon */ , 29, CMMI10BGR, VARIABLE, NULL},
    {"f" /*\phi */ , 30, CMMI10BGR, VARIABLE, NULL},
    {"x" /*\chi */ , 31, CMMI10BGR, VARIABLE, NULL},
    {"y" /*\psi */ , 32, CMMI10BGR, VARIABLE, NULL},
    {"w" /*\omega */ , 33, CMMI10BGR, VARIABLE, NULL},
#if 0
    {"?" /*\varepsilon */ , 34, CMMI10BGR, VARIABLE, NULL},
    {"?" /*\vartheta */ , 35, CMMI10BGR, VARIABLE, NULL},
    {"?" /*\varpi */ , 36, CMMI10BGR, VARIABLE, NULL},
    {"?" /*\varrho */ , 37, CMMI10BGR, VARIABLE, NULL},
    {"?" /*\varsigma */ , 38, CMMI10BGR, VARIABLE, NULL},
    {"?" /*\varphi */ , 39, CMMI10BGR, VARIABLE, NULL},
#endif
    /* ------------------ B B O L D G R -----------------------
       Using "Beta code" <http://en.wikipedia.org/wiki/Beta_code>
       to represent Greek characters in latin, e.g., type a to get
       \alpha, etc.
       symbol     charnum    family    class     function
       -------------------------------------------------------- */
    /* --- uppercase greek letters --- */
    {"G" /*\Gamma */ , 0, BBOLD10GR, VARIABLE, NULL},
    {"D" /*\Delta */ , 1, BBOLD10GR, VARIABLE, NULL},
    {"Q" /*\Theta */ , 2, BBOLD10GR, VARIABLE, NULL},
    {"L" /*\Lambda */ , 3, BBOLD10GR, VARIABLE, NULL},
    {"C" /*\Xi */ , 4, BBOLD10GR, VARIABLE, NULL},
    {"P" /*\Pi */ , 5, BBOLD10GR, VARIABLE, NULL},
    {"S" /*\Sigma */ , 6, BBOLD10GR, VARIABLE, NULL},
    {"U" /*\Upsilon */ , 7, BBOLD10GR, VARIABLE, NULL},
    {"F" /*\Phi */ , 8, BBOLD10GR, VARIABLE, NULL},
    {"Y" /*\Psi */ , 9, BBOLD10GR, VARIABLE, NULL},
    {"W" /*\Omega */ , 10, BBOLD10GR, VARIABLE, NULL},
    /* --- lowercase greek letters --- */
    {"a" /*\alpha */ , 11, BBOLD10GR, VARIABLE, NULL},
    {"b" /*\beta */ , 12, BBOLD10GR, VARIABLE, NULL},
    {"g" /*\gamma */ , 13, BBOLD10GR, VARIABLE, NULL},
    {"d" /*\delta */ , 14, BBOLD10GR, VARIABLE, NULL},
    {"e" /*\epsilon */ , 15, BBOLD10GR, VARIABLE, NULL},
    {"z" /*\zeta */ , 16, BBOLD10GR, VARIABLE, NULL},
    {"h" /*\eta */ , 17, BBOLD10GR, VARIABLE, NULL},
    {"q" /*\theta */ , 18, BBOLD10GR, VARIABLE, NULL},
    {"i" /*\iota */ , 19, BBOLD10GR, VARIABLE, NULL},
    {"k" /*\kappa */ , 20, BBOLD10GR, VARIABLE, NULL},
    {"l" /*\lambda */ , 21, BBOLD10GR, VARIABLE, NULL},
    {"m" /*\mu */ , 22, BBOLD10GR, VARIABLE, NULL},
    {"n" /*\nu */ , 23, BBOLD10GR, VARIABLE, NULL},
    {"c" /*\xi */ , 24, BBOLD10GR, VARIABLE, NULL},
    {"p" /*\pi */ , 25, BBOLD10GR, VARIABLE, NULL},
    {"r" /*\rho */ , 26, BBOLD10GR, VARIABLE, NULL},
    {"s" /*\sigma */ , 27, BBOLD10GR, VARIABLE, NULL},
    {"t" /*\tau */ , 28, BBOLD10GR, VARIABLE, NULL},
    {"u" /*\upsilon */ , 29, BBOLD10GR, VARIABLE, NULL},
    {"f" /*\phi */ , 30, BBOLD10GR, VARIABLE, NULL},
    {"x" /*\chi */ , 31, BBOLD10GR, VARIABLE, NULL},
    {"y" /*\psi */ , 32, BBOLD10GR, VARIABLE, NULL},
    {"w" /*\omega */ , 127, BBOLD10GR, VARIABLE, NULL},
    /* --- trailer record --- */
    {NULL, -999, -999, -999, NULL}
};


/* -------------------------------------------------------------------------
miscellaneous macros
-------------------------------------------------------------------------- */
#define compress(s,c) if((s)!=NULL) /* remove embedded c's from s */ \
    { char *p; while((p=strchr((s),(c)))!=NULL) {strsqueeze(p,1);} } else
#define slower(s)  if ((s)!=NULL)   /* lowercase all chars in s */ \
    { char *p=(s); while(*p!='\0'){*p=tolower(*p); p++;} } else
                                                                /*subraster *subrastcpy(); *//* need global module declaration */
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

/* ============================================================================= */

static int aagridnum(raster * rp, int irow, int icol);
static int aapattern1124(raster * rp, int irow, int icol, int gridnum,
                         int grayscale);
static int aapattern19(raster * rp, int irow, int icol, int gridnum,
                       int grayscale);
static int aapattern20(raster * rp, int irow, int icol, int gridnum,
                       int grayscale);
static int aapattern39(raster * rp, int irow, int icol, int gridnum,
                       int grayscale);
static int aapatternnum(int gridnum);
static int aapatterns(raster * rp, int irow, int icol, int gridnum,
                      int patternum, int grayscale);
static int aasupsamp(raster * rp, raster ** aa, int sf, int grayscale);
static raster *aaweights(int width, int height);

static subraster *arrow_subraster(int width, int height, int pixsz,
                                  int drctn, int isBig);
static int circle_recurse(raster * rp, int row0, int col0, int row1,
                          int col1, int thickness, double theta0,
                          double theta1);
int delete_raster(raster * rp);
int delete_subraster(subraster * sp);
static char *findbraces(const char *expression, const char *command);
static int get_baseline(chardef * gfdata);
static subraster *get_delim(const char *symbol, int height, int family);
static int get_ligature(const char *expression, int family);
static int getstore(STORE * store, const char *identifier);
static raster *gftobitmap(raster * gf);
static int isbrace(const char *expression, const char *braces,
                   int isescape);
static int line_raster(raster * rp, int row0, int col0, int row1, int col1,
                       int thickness);
static int line_recurse(raster * rp, double row0, double col0, double row1,
                        double col1, int thickness);
subraster *make_delim(const char *symbol, int height);
static subraster *rastdispmath(const char **expression, int size,
                               subraster * sp);
static subraster *rastlimits(const char **expression, int size,
                             subraster * basesp);
static int rastsmash(subraster * sp1, subraster * sp2);
static subraster *rastparen(const char **subexpr, int size,
                            subraster * basesp);
static subraster *rastscripts(const char **expression, int size,
                              subraster * basesp);
static int rule_raster(raster * rp, int top, int left, int width,
                       int height, int type);
static char *strchange(int nfirst, char *from, const char *to);
int strreplace(char *string, const char *from, const char *to,
               int nreplace);
char *strtexchr(const char *string, const char *texchr);
char *strwrap(const char *s, int linelen, int tablen);
char *strwstr(const char *string, const char *substr, const char *white,
              int *sublen);
char *texleft(const char *expression, char *subexpr, int maxsubsz,
              char *ldelim, char *rdelim);
int type_raster(raster * rp, FILE * fp);

/* ============================================================================= */

#if 1
/* ==========================================================================
 * Function:    new_raster ( width, height, pixsz )
 * Purpose: Allocation and constructor for raster.
 *      mallocs and initializes memory for width*height pixels,
 *      and returns raster struct ptr to caller.
 * --------------------------------------------------------------------------
 * Arguments:   width (I)   int containing width, in bits,
 *              of raster pixmap to be allocated
 *      height (I)  int containing height, in bits/scans,
 *              of raster pixmap to be allocated
 *      pixsz (I)   int containing #bits per pixel, 1 or 8
 * --------------------------------------------------------------------------
 * Returns: ( raster * )    ptr to allocated and initialized
 *              raster struct, or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:
 * ======================================================================= */

raster *new_raster(int width, int height, int pixsz)
{
    raster *rp = NULL;          /* raster ptr returned to caller */
    pixbyte *pixmap = NULL;     /* raster pixel map to be malloced */
    int nbytes = pixsz * bitmapsz(width, height);       /* #bytes needed for pixmap */
    int filler = (isstring ? ' ' : 0);  /* pixmap filler */
    int npadding = (0 && issupersampling ? 8 + 256 : 0);        /* padding bytes */

    if (msgfp != NULL && msglevel >= 9999) {
        fprintf(msgfp, "new_raster(%d,%d,%d)> entry point\n",
                width, height, pixsz);
        fflush(msgfp);
    }
/* --- allocate and initialize raster struct --- */
    rp = (raster *) malloc(sizeof(raster));     /* malloc raster struct */
    if (msgfp != NULL && msglevel >= 9999) {
        fprintf(msgfp, "new_raster> rp=malloc(%d) returned (%s)\n",
                (int)sizeof(raster), (rp == NULL ? "null ptr" : "success"));
        fflush(msgfp);
    }
    if (rp == NULL) {  /* malloc failed */
        goto end_of_job;        /* return error to caller */
    }
    rp->width = width;          /* store width in raster struct */
    rp->height = height;        /* and store height */
    rp->format = 1;             /* initialize as bitmap format */
    rp->pixsz = pixsz;          /* store #bits per pixel */
    rp->pixmap = NULL;      /* init bitmap as null ptr */
/* --- allocate and initialize bitmap array --- */
    if (msgfp != NULL && msglevel >= 9999) {
        fprintf(msgfp, "new_raster> calling pixmap=malloc(%d)\n", nbytes);
        fflush(msgfp);
    }
    if (nbytes > 0 && nbytes <= pixsz * maxraster)      /* fail if width*height too big */
        pixmap = (pixbyte *) malloc(nbytes + npadding); /*bytes for width*height bits */
    if (msgfp != NULL && msglevel >= 9999) {
        fprintf(msgfp, "new_raster> pixmap=malloc(%d) returned (%s)\n",
                nbytes, (pixmap == NULL ? "null ptr" : "success"));
        fflush(msgfp);
    }
    if (pixmap == NULL) {   /* malloc failed */
        delete_raster(rp);      /* so free everything */
        rp = NULL;   /* reset pointer */
        goto end_of_job;
    }                           /* and return error to caller */
    memset((void *) pixmap, filler, nbytes);    /* init bytes to binary 0's or ' 's */
    *pixmap = 0;      /* and first byte alwasy 0 */
    rp->pixmap = pixmap;        /* store ptr to malloced memory */
/* -------------------------------------------------------------------------
Back to caller with address of raster struct, or NULL ptr for any error.
-------------------------------------------------------------------------- */
  end_of_job:
    if (msgfp != NULL && msglevel >= 9999) {
        fprintf(msgfp, "new_raster(%d,%d,%d)> returning (%s)\n",
                width, height, pixsz,
                (rp == NULL ? "null ptr" : "success"));
        fflush(msgfp);
    }
    return rp;
}


/* ==========================================================================
 * Function:    new_subraster ( width, height, pixsz )
 * Purpose: Allocate a new subraster along with
 *      an embedded raster of width x height.
 * --------------------------------------------------------------------------
 * Arguments:   width (I)   int containing width of embedded raster
 *      height (I)  int containing height of embedded raster
 *      pixsz (I)   int containing #bits per pixel, 1 or 8
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to newly-allocated subraster,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o if width or height <=0, embedded raster not allocated
 * ======================================================================= */
subraster *new_subraster(int width, int height, int pixsz)
{
    subraster *sp = NULL;       /* subraster returned to caller */
    raster *rp = NULL;          /* image raster embedded in sp */
    int size = NORMALSIZE;      /* default size */
    int baseline = height - 1;  /* and baseline */
/* -------------------------------------------------------------------------
allocate and initialize subraster struct
-------------------------------------------------------------------------- */
    if (msgfp != NULL && msglevel >= 9999) {
        fprintf(msgfp, "new_subraster(%d,%d,%d)> entry point\n",
                width, height, pixsz);
        fflush(msgfp);
    }
/* --- allocate subraster struct --- */
    sp = (subraster *) malloc(sizeof(subraster));       /* malloc subraster struct */
    if (sp == NULL)             /* malloc failed */
        goto end_of_job;        /* return error to caller */
/* --- initialize subraster struct --- */
    sp->type = NOVALUE;         /* character or image raster */
    sp->symdef = NULL;          /* mathchardef identifying image */
    sp->baseline = baseline;    /*0 if image is entirely descending */
    sp->size = size;            /* font size 0-4 */
    sp->toprow = sp->leftcol = (-1);    /* upper-left corner of subraster */
    sp->image = NULL;           /*ptr to bitmap image of subraster */
/* -------------------------------------------------------------------------
allocate raster and embed it in subraster, and return to caller
-------------------------------------------------------------------------- */
/* --- allocate raster struct if desired --- */
    if (width > 0 && height > 0 && pixsz > 0) { /* caller wants raster */
        if ((rp = new_raster(width, height, pixsz))     /* allocate embedded raster */
            !=NULL)             /* if allocate succeeded */
            sp->image = rp;     /* embed raster in subraster */
        else {                  /* or if allocate failed */
            delete_subraster(sp);       /* free non-unneeded subraster */
            sp = NULL;
        }
    }
    /* signal error */
    /* --- back to caller with new subraster or NULL --- */
  end_of_job:
    if (msgfp != NULL && msglevel >= 9999) {
        fprintf(msgfp, "new_subraster(%d,%d,%d)> returning (%s)\n",
                width, height, pixsz,
                (sp == NULL ? "null ptr" : "success"));
        fflush(msgfp);
    }
    return sp;
}

/* ==========================================================================
 * Function:    new_chardef (  )
 * Purpose: Allocates and initializes a chardef struct,
 *      but _not_ the embedded raster struct.
 * --------------------------------------------------------------------------
 * Arguments:   none
 * --------------------------------------------------------------------------
 * Returns: ( chardef * )   ptr to allocated and initialized
 *              chardef struct, or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:
 * ======================================================================= */
/* --- entry point --- */
chardef *new_chardef(void)
{
    chardef *cp = NULL;         /* chardef ptr returned to caller */
/* -------------------------------------------------------------------------
allocate and initialize chardef struct
-------------------------------------------------------------------------- */
    cp = (chardef *) malloc(sizeof(chardef));   /* malloc chardef struct */
    if (cp == NULL)             /* malloc failed */
        goto end_of_job;        /* return error to caller */
    cp->charnum = cp->location = 0;     /* init character description */
    cp->toprow = cp->topleftcol = 0;    /* init upper-left corner */
    cp->botrow = cp->botleftcol = 0;    /* init lower-left corner */
    cp->image.width = cp->image.height = 0;     /* init raster dimensions */
    cp->image.format = 0;       /* init raster format */
    cp->image.pixsz = 0;        /* and #bits per pixel */
    cp->image.pixmap = NULL;    /* init raster pixmap as null */
/* -------------------------------------------------------------------------
Back to caller with address of chardef struct, or NULL ptr for any error.
-------------------------------------------------------------------------- */
  end_of_job:
    return cp;
}

/* ==========================================================================
 * Function:    delete_raster ( rp )
 * Purpose: Destructor for raster.
 *      Frees memory for raster bitmap and struct.
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      ptr to raster struct to be deleted.
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if completed successfully,
 *              or 0 otherwise (for any error).
 * --------------------------------------------------------------------------
 * Notes:
 * ======================================================================= */
int delete_raster(raster * rp)
{
    if (rp != NULL) {           /* can't free null ptr */
        if (rp->pixmap != NULL) {       /* can't free null ptr */
            free((void *) rp->pixmap);  /* free pixmap within raster */
        }
        free((void *) rp);      /* lastly, free raster struct */
    }
    return 1;
}

/* ==========================================================================
 * Function:    delete_subraster ( sp )
 * Purpose: Deallocates a subraster (and embedded raster)
 * --------------------------------------------------------------------------
 * Arguments:   sp (I)      ptr to subraster struct to be deleted.
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if completed successfully,
 *              or 0 otherwise (for any error).
 * --------------------------------------------------------------------------
 * Notes:
 * ======================================================================= */
int delete_subraster(subraster * sp)
{
    if (sp != (subraster *) NULL) {     /* can't free null ptr */
        if (sp->type != CHARASTER) {    /* not static character data */
            if (sp->image != NULL) {    /*raster allocated within subraster */
                delete_raster(sp->image);       /* so free embedded raster */
            }
        }
        free((void *) sp);      /* and free subraster struct itself */
    }
    return 1;
}

#if 0
/* ==========================================================================
 * Function:    delete_chardef ( cp )
 * Purpose: Deallocates a chardef (and bitmap of embedded raster)
 * --------------------------------------------------------------------------
 * Arguments:   cp (I)      ptr to chardef struct to be deleted.
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if completed successfully,
 *              or 0 otherwise (for any error).
 * --------------------------------------------------------------------------
 * Notes:
 * ======================================================================= */
static int delete_chardef(chardef * cp)
{
    if (cp != NULL) {           /* can't free null ptr */
        if (cp->image.pixmap != NULL) { /* pixmap allocated within raster */
            free((void *) cp->image.pixmap);    /* so free embedded pixmap */
        }
        free((void *) cp);      /* and free chardef struct itself */
    }
    return 1;
}
#endif

/* ==========================================================================
 * Function:    rastcpy ( rp )
 * Purpose: makes duplicate copy of rp
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      ptr to raster struct to be copied
 * --------------------------------------------------------------------------
 * Returns: ( raster * )    ptr to new copy rp,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
/* --- entry point --- */
raster *rastcpy(raster * rp)
{
    raster *newrp = NULL;       /*copied raster returned to caller */
    int height = (rp == NULL ? 0 : rp->height); /* original and copied height */
    int width = (rp == NULL ? 0 : rp->width);   /* original and copied width */
    int pixsz = (rp == NULL ? 0 : rp->pixsz);   /* #bits per pixel */
    int nbytes = (rp == NULL ? 0 : (pixmapsz(rp)));     /* #bytes in rp's pixmap */
/* -------------------------------------------------------------------------
allocate rotated raster and fill it
-------------------------------------------------------------------------- */
/* --- allocate copied raster with same width,height, and copy bitmap --- */
    if (rp != NULL) {           /* nothing to copy if ptr null */
        if ((newrp = new_raster(width, height, pixsz)) != NULL) {
            memcpy(newrp->pixmap, rp->pixmap, nbytes);  /* fill copied raster pixmap */
        }
    }
    return newrp;
}

/* ==========================================================================
 * Function:    subrastcpy ( sp )
 * Purpose: makes duplicate copy of sp
 * --------------------------------------------------------------------------
 * Arguments:   sp (I)      ptr to subraster struct to be copied
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to new copy sp,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
subraster *subrastcpy(subraster * sp)
{
    subraster *newsp = NULL;    /* allocate new subraster */
    raster *newrp = NULL;       /* and new raster image within it */
/* -------------------------------------------------------------------------
make copy, and return it to caller
-------------------------------------------------------------------------- */
    if (sp == NULL) {
        goto end_of_job;        /* nothing to copy */
    }
/* --- allocate new subraster "envelope" for copy --- */
    if ((newsp = new_subraster(0, 0, 0)) == NULL) {
        goto end_of_job;        /* and quit if we fail to allocate */
    }
/* --- transparently copy original envelope to new one --- */
    memcpy((void *) newsp, (void *) sp, sizeof(subraster));     /* copy envelope */
/* --- make a copy of the rasterized image itself, if there is one --- */
    if (sp->image != NULL) {    /* there's an image embedded in sp */
        if ((newrp = rastcpy(sp->image)) == NULL) {     /* failed to copy successfully */
            delete_subraster(newsp);    /* won't need newsp any more */
            newsp = NULL;       /* because we're returning error */
            goto end_of_job;
        }
    }
    /* back to caller with error signal */
    /* --- set new params in new envelope --- */
    newsp->image = newrp;       /* new raster image we just copied */
    switch (sp->type) {         /* set new raster image type */
    case STRINGRASTER:
    case CHARASTER:
        newsp->type = STRINGRASTER;
        break;
    case ASCIISTRING:
        newsp->type = ASCIISTRING;
        break;
    case FRACRASTER:
        newsp->type = FRACRASTER;
        break;
    case BLANKSIGNAL:
        newsp->type = blanksignal;
        break;
    case IMAGERASTER:
    default:
        newsp->type = IMAGERASTER;
        break;
    }
/* --- return copy of sp to caller --- */
  end_of_job:
    return newsp;
}

/* ==========================================================================
 * Function:    rastrot ( rp )
 * Purpose: rotates rp image 90 degrees right/clockwise
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      ptr to raster struct to be rotated
 * --------------------------------------------------------------------------
 * Returns: ( raster * )    ptr to new raster rotated relative to rp,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o An underbrace is } rotated 90 degrees clockwise,
 *      a hat is <, etc.
 * ======================================================================= */
raster *rastrot(raster * rp)
{
    raster *rotated = NULL;     /*rotated raster returned to caller */
    int height = rp->height, irow;      /* original height, row index */
    int width = rp->width, icol;        /* original width, column index */
    int pixsz = rp->pixsz;      /* #bits per pixel */
/* -------------------------------------------------------------------------
allocate rotated raster and fill it
-------------------------------------------------------------------------- */
/* --- allocate rotated raster with flipped width<-->height --- */
    if ((rotated = new_raster(height, width, pixsz)) != NULL) { /* flip width,height */
        /* check that allocation succeeded */
        /* --- fill rotated raster --- */
        for (irow = 0; irow < height; irow++) { /* for each row of rp */
            for (icol = 0; icol < width; icol++) {      /* and each column of rp */
                int value = getpixel(rp, irow, icol);
                /* setpixel(rotated,icol,irow,value); } */
                setpixel(rotated, icol, (height - 1 - irow), value);
            }
        }
    }
    return rotated;
}

/* ==========================================================================
 * Purpose: magnifies rp by integer magstep,
 *      e.g., double-height and double-width if magstep=2
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      ptr to raster struct to be "magnified"
 *      magstep (I) int containing magnification scale,
 *              e.g., 2 to double the width and height of rp
 * --------------------------------------------------------------------------
 * Returns: ( raster * )    ptr to new raster magnified relative to rp,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
raster *rastmag(raster * rp, int a_magstep)
{
    raster *magnified = NULL;   /* magnified raster back to caller */
    int height = rp->height, irow,      /* height, row index */
        width = rp->width, icol,        /* width, column index */
        mrow = 0, mcol = 0,     /* dup pixels magstep*magstep times */
        pixsz = rp->pixsz;      /* #bits per pixel */
/* -------------------------------------------------------------------------
check args
-------------------------------------------------------------------------- */
    if (rp == NULL) {
        goto end_of_job;        /* no input raster supplied */
    }
    if (a_magstep < 1 || a_magstep > 10) {
        goto end_of_job;        /* sanity check */
    }
/* -------------------------------------------------------------------------
allocate magnified raster and fill it
-------------------------------------------------------------------------- */
/* --- allocate magnified raster with magstep*width, magstep*height --- */
    if ((magnified =
         new_raster(a_magstep * width, a_magstep * height,
                    pixsz)) != NULL) {
        /* check that allocation succeeded */
        /* --- fill reflected raster --- */
        for (irow = 0; irow < height; irow++) { /* for each row of rp */
            for (mrow = 0; mrow < a_magstep; mrow++) {  /* dup row magstep times */
                for (icol = 0; icol < width; icol++) {  /* and for each column of rp */
                    for (mcol = 0; mcol < a_magstep; mcol++) {  /* dup col magstep times */
                        int value = getpixel(rp, irow, icol);
                        int row1 = irow * a_magstep;
                        int col1 = icol * a_magstep;
                        setpixel(magnified, (row1 + mrow), (col1 + mcol),
                                 value);
                    }
                }
            }
        }
    }

  end_of_job:
    return magnified;
}

/* ==========================================================================
 * Function:    bytemapmag ( bytemap, width, height, magstep )
 * Purpose: magnifies a bytemap by integer magstep,
 *      e.g., double-height and double-width if magstep=2
 * --------------------------------------------------------------------------
 * Arguments:   bytemap (I) intbyte * ptr to byte map to be "magnified"
 *      width (I)   int containing #cols in original bytemap
 *      height (I)  int containing #rows in original bytemap
 *      magstep (I) int containing magnification scale,
 *              e.g., 2 to double the width and height of rp
 * --------------------------------------------------------------------------
 * Returns: ( intbyte * )   ptr to new bytemap magnified relative to
 *              original bytemap, or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o Apply EPX/Scale2x/AdvMAME2x  for magstep 2,
 *      and Scale3x/AdvMAME3x  for magstep 3,
 *      as described by http://en.wikipedia.org/wiki/2xSaI
 * ======================================================================= */
intbyte *bytemapmag(intbyte * bytemap, int width, int height,
                    int a_magstep)
{
    intbyte *magnified = NULL;  /* magnified bytemap back to caller */
    int irow, icol;             /* original height, width indexes */
    int mrow = 0, mcol = 0;     /* dup bytes magstep*magstep times */
    int imap = -1;              /* original bytemap[] index */
    int byteval = 0;            /* byteval=bytemap[imap] */
    int isAdvMAME = 1;          /* true to apply AdvMAME2x and 3x */
    int icell[10];              /* bytemap[] nearest neighbors */
    int bmmdiff = 64;           /* nearest neighbor diff allowed */
#define bmmeq(i,j) ((absval((icell[i]-icell[j]))<=bmmdiff))     /*approx equal */
/* -------------------------------------------------------------------------
check args
-------------------------------------------------------------------------- */
    if (bytemap == NULL) {
        goto end_of_job;        /* no input bytemap supplied */
    }
    if (width < 1 || height < 1) {
        goto end_of_job;        /* invalid bytemap dimensions */
    }
    if (width * height > 100000) {
        goto end_of_job;        /* sanity check */
    }
    if (a_magstep < 1 || a_magstep > 10) {
        goto end_of_job;        /* sanity check */
    }
/* -------------------------------------------------------------------------
allocate magnified bytemap and fill it
-------------------------------------------------------------------------- */
/* --- allocate bytemap for magstep*width, magstep*height --- */
    if ((magnified =
         (intbyte *) (malloc(a_magstep * width * a_magstep * height))) !=
        NULL) {
        /* check that allocation succeeded */
        /* --- fill reflected raster --- */
        for (irow = 0; irow < height; irow++) { /* for each row of bytemap */
            for (icol = 0; icol < width; icol++) {      /* and for each column of bytemap */
                int imag1 = (icol + irow * (width * a_magstep)) * a_magstep;    /*upper-left corner */
                imap++;         /* bump bytemap[] index */
                byteval = (int) (bytemap[imap]);        /* grayscale value at this pixel */
                for (mrow = 0; mrow < a_magstep; mrow++) {      /* dup row magstep times */
                    for (mcol = 0; mcol < a_magstep; mcol++) {  /* dup col magstep times */
                        int idup = mcol + mrow * (width * a_magstep);   /* offset from imag1 */
                        int imag = imag1 + idup;        /* adjust magnified[imag] */
                        magnified[imag] = (intbyte) (byteval);
                        /* --- apply AdvMAME2x and 3x (if desired) --- */
                        if (isAdvMAME) {        /* AdvMAME2x and 3x wanted */
                            int mcell = 1 + mcol + a_magstep * mrow;    /*1,2,3,4 or 1,2,3,4,5,6,7,8,9 */
                            icell[5] = byteval; /* center cell of 3x3 bytemap[] */
                            icell[4] = (icol > 0 ? (int) (bytemap[imap - 1]) : byteval);        /*left of center */
                            icell[6] = (icol < width ? (int) (bytemap[imap + 1]) : byteval);    /*right */
                            icell[2] = (irow > 0 ? (int) (bytemap[imap - width]) : byteval);    /*above center */
                            icell[8] = (irow < height ? (int) (bytemap[imap + width]) : byteval);       /*below */
                            icell[1] = (irow > 0
                                        && icol >
                                        0
                                        ? (int) (bytemap[imap - width - 1])
                                        : byteval);
                            icell[3] = (irow > 0
                                        && icol <
                                        width
                                        ? (int) (bytemap[imap - width + 1])
                                        : byteval);
                            icell[7] = (irow < height
                                        && icol >
                                        0
                                        ? (int) (bytemap[imap + width - 1])
                                        : byteval);
                            icell[9] = (irow < height
                                        && icol <
                                        width
                                        ? (int) (bytemap[imap + width + 1])
                                        : byteval);
                            switch (a_magstep) {        /* 2x magstep=2, 3x magstep=3 */
                            default:
                                break;  /* no AdvMAME at other magsteps */
                            case 2:    /* AdvMAME2x */
                                if (mcell == 1)
                                    if (bmmeq(4, 2) && !bmmeq(4, 8)
                                        && !bmmeq(2, 6))
                                        magnified[imag] = icell[2];
                                if (mcell == 2)
                                    if (bmmeq(2, 6) && !bmmeq(2, 4)
                                        && !bmmeq(6, 8))
                                        magnified[imag] = icell[6];
                                if (mcell == 4)
                                    if (bmmeq(6, 8) && !bmmeq(6, 2)
                                        && !bmmeq(8, 4))
                                        magnified[imag] = icell[8];
                                if (mcell == 3)
                                    if (bmmeq(8, 4) && !bmmeq(8, 6)
                                        && !bmmeq(4, 2))
                                        magnified[imag] = icell[4];
                                break;
                            case 3:    /* AdvMAME3x */
                                if (mcell == 1)
                                    if (bmmeq(4, 2) && !bmmeq(4, 8)
                                        && !bmmeq(2, 6))
                                        magnified[imag] = icell[4];
                                if (mcell == 2)
                                    if ((bmmeq(4, 2) && !bmmeq(4, 8)
                                         && !bmmeq(2, 6) && !bmmeq(5, 3))
                                        || (bmmeq(2, 6) && !bmmeq(2, 4)
                                            && !bmmeq(6, 8)
                                            && !bmmeq(5, 1)))
                                        magnified[imag] = icell[2];
                                if (mcell == 3)
                                    if (bmmeq(2, 6) && !bmmeq(2, 4)
                                        && !bmmeq(6, 8))
                                        magnified[imag] = icell[6];
                                if (mcell == 4)
                                    if ((bmmeq(8, 4) && !bmmeq(8, 6)
                                         && !bmmeq(4, 2) && !bmmeq(5, 1))
                                        || (bmmeq(4, 2) && !bmmeq(4, 8)
                                            && !bmmeq(2, 6)
                                            && !bmmeq(5, 7)))
                                        magnified[imag] = icell[4];
                                if (mcell == 6)
                                    if ((bmmeq(2, 6) && !bmmeq(2, 4)
                                         && !bmmeq(6, 8) && !bmmeq(5, 9))
                                        || (bmmeq(6, 8) && !bmmeq(6, 2)
                                            && !bmmeq(8, 4)
                                            && !bmmeq(5, 3)))
                                        magnified[imag] = icell[6];
                                if (mcell == 7)
                                    if (bmmeq(8, 4) && !bmmeq(8, 6)
                                        && !bmmeq(4, 2))
                                        magnified[imag] = icell[4];
                                if (mcell == 8)
                                    if ((bmmeq(6, 8) && !bmmeq(6, 2)
                                         && !bmmeq(8, 4) && !bmmeq(5, 7))
                                        || (bmmeq(8, 4) && !bmmeq(8, 6)
                                            && !bmmeq(4, 2)
                                            && !bmmeq(5, 9)))
                                        magnified[imag] = icell[8];
                                if (mcell == 9)
                                    if (bmmeq(6, 8) && !bmmeq(6, 2)
                                        && !bmmeq(8, 4))
                                        magnified[imag] = icell[6];
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
  end_of_job:
    return magnified;
#undef bmmeq
}

/* ==========================================================================
 * Function:    rastref ( rp, axis )
 * Purpose: reflects rp, horizontally about y-axis |_ becomes _| if axis=1
 *      or vertically about x-axis M becomes W if axis=2.
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      ptr to raster struct to be reflected
 *      axis (I)    int containing 1 for horizontal reflection,
 *              or 2 for vertical
 * --------------------------------------------------------------------------
 * Returns: ( raster * )    ptr to new raster reflected relative to rp,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static raster *rastref(raster * rp, int axis)
{
    raster *reflected = NULL;   /* reflected raster back to caller */
    int height = rp->height, irow,      /* height, row index */
        width = rp->width, icol,        /* width, column index */
        pixsz = rp->pixsz;      /* #bits per pixel */
/* -------------------------------------------------------------------------
allocate reflected raster and fill it
-------------------------------------------------------------------------- */
/* --- allocate reflected raster with same width, height --- */
    if (axis == 1 || axis == 2) {       /* first validate axis arg */
        if ((reflected = new_raster(width, height, pixsz)) != NULL) {
            /* check that allocation succeeded */
            /* --- fill reflected raster --- */
            for (irow = 0; irow < height; irow++) {     /* for each row of rp */
                for (icol = 0; icol < width; icol++) {  /* and each column of rp */
                    int value = getpixel(rp, irow, icol);
                    if (axis == 1) {
                        setpixel(reflected, irow, width - 1 - icol, value);
                    }
                    if (axis == 2) {
                        setpixel(reflected, height - 1 - irow, icol,
                                 value);
                    }
                }
            }
        }
    }
    return reflected;
}

/* ==========================================================================
 * Function:    rastput ( target, source, top, left, isopaque )
 * Purpose: Overlays source onto target,
 *      with the 0,0-bit of source onto the top,left-bit of target.
 * --------------------------------------------------------------------------
 * Arguments:   target (I)  ptr to target raster struct
 *      source (I)  ptr to source raster struct
 *      top (I)     int containing 0 ... target->height - 1
 *      left (I)    int containing 0 ... target->width - 1
 *      isopaque (I)    int containing false (zero) to allow
 *              original 1-bits of target to "show through"
 *              0-bits of source.
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if completed successfully,
 *              or 0 otherwise (for any error).
 * --------------------------------------------------------------------------
 * Notes:
 * ======================================================================= */
static int rastput(raster * target, raster * source, int top, int left,
                   int isopaque)
{
    int irow, icol,             /* indexes over source raster */
     twidth = target->width, theight = target->height,  /*target width,height */
        tpix, ntpix = twidth * theight; /* #pixels in target */
    int isfatal = 0,            /* true to abend on out-of-bounds error */
        isstrict = 0 /*1 */ ,   /* true for strict bounds check - no "wrap" */
        isokay = 1;             /* true if no pixels out-of-bounds */
/* -------------------------------------------------------------------------
superimpose source onto target, one bit at a time
-------------------------------------------------------------------------- */
    if (isstrict && (top < 0 || left < 0)) {    /* args fail strict test */
        isokay = 0;             /* so just return error */
    } else {
        for (irow = 0; irow < source->height; irow++) { /* for each scan line */
            tpix = (top + irow) * target->width + left - 1;     /*first target pixel (-1) */
            for (icol = 0; icol < source->width; icol++) {      /* each pixel in scan line */
                int svalue = getpixel(source, irow, icol);      /* source pixel value */
                ++tpix;         /* bump target pixel */
                if (msgfp != NULL && msglevel >= 9999) {        /* debugging output */
                    fprintf(msgfp,
                            "rastput> tpix,ntpix=%d,%d top,irow,theight=%d,%d,%d "
                            "left,icol,twidth=%d,%d,%d\n", tpix, ntpix,
                            top, irow, theight, left, icol, twidth);
                    fflush(msgfp);
                }
                if (tpix >= ntpix
                    || (isstrict
                        && (irow + top >= theight
                            || icol + left >= twidth))) {
                    /* bounds check failed */
                    isokay = 0; /* reset okay flag */
                    if (isfatal) {
                        goto end_of_job;        /* abort if error is fatal */
                    } else {
                        break;
                    }
                }               /*or just go on to next row */
                if (tpix >= 0) {        /* bounds check okay */
                    if (svalue != 0 || isopaque) {      /*got dark or opaque source */
                        setpixel(target, irow + top, icol + left, svalue);
                    }           /*overlay source on targ */
                }
            }
        }
    }
  end_of_job:
    return isokay;
}

/* ==========================================================================
 * Function:    rastcompose ( sp1, sp2, offset2, isalign, isfree )
 * Purpose: Overlays sp2 on top of sp1, leaving both unchanged
 *      and returning a newly-allocated composite subraster.
 *      Frees/deletes input sp1 and/or sp2 depending on value
 *      of isfree (0=none, 1=sp1, 2=sp2, 3=both).
 * --------------------------------------------------------------------------
 * Arguments:   sp1 (I)     subraster *  to "underneath" subraster,
 *              whose baseline is preserved
 *      sp2 (I)     subraster *  to "overlaid" subraster
 *      offset2 (I) int containing 0 or number of pixels
 *              to horizontally shift sp2 relative to sp1,
 *              either positive (right) or negative
 *      isalign (I) int containing 1 to align baselines,
 *              or 0 to vertically center sp2 over sp1.
 *              For isalign=2, images are vertically
 *              centered, but then adjusted by \raisebox
 *              lifts, using global variables rastlift1
 *              for sp1 and rastlift for sp2.
 *      isfree (I)  int containing 1=free sp1 before return,
 *              2=free sp2, 3=free both, 0=free none.
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) pointer to constructed subraster
 *              or  NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o The top-left corner of each raster box has coords (0,0),
 *      down to (h-1,w-1) for a box of height h and width w.
 *        o A raster's baseline, b, is typically 0 <= b < h.
 *      But b can actually go out-of-bounds, b>=h or b<0, for
 *      an image additionally lifted (b>=h) or lowered (b<0)
 *      with respect to the surrounding expression.
 *        o Note that b=h-1 means no descenders and the bottom
 *      of the symbol rests exactly on the baseline,
 *      whereas b=0 means the top pixel of the symbol rests
 *      on the baseline, and all other pixels are descenders.
 *        o The composite raster is constructed as follows...
 *      The base image is labelled height h1 and baseline b1,
 *      the overlay h2 and b2, and the composite H and B.
 *           base       overlay
 *      --- +------------------------+ ---   For the overlay to be
 *       ^  |   ^        +----------+|  ^    vertically centered with
 *       |  |   |        |          ||  |    respect to the base,
 *       |  |   |B-b1    |          ||  |      B - b1 = H-B -(h1-b1), so
 *       |  |   v        |          ||  |      2*B = H-h1 + 2*b1
 *       |  |+----------+|          ||  |      B = b1 + (H-h1)/2
 *       B  ||  ^    ^  ||          ||  |    And when the base image is
 *       |  ||  |    |  ||          ||  |    bigger, H=h1 and B=b1 is
 *       |  ||  b1   |  ||          ||  |    the obvious correct answer.
 *       |  ||  |    h1 ||          || H=h2
 *       v  ||  v    |  ||          ||  |
 *    ----------||-------|--||          ||--|--------
 *    baseline  || h1-b1 v  || overlay  ||  |
 *    for base  |+----------+| baseline ||  |
 *    and com-  |   ^        | ignored  ||  |
 *    posite    |   |H-B-    |----------||  |
 *      |   | (h1-b1)|          ||  |
 *      |   v        +----------+|  v
 *      +------------------------+ ---
 * ======================================================================= */
static subraster *rastcompose(subraster * sp1, subraster * sp2,
                              int offset2, int isalign, int isfree)
{
    subraster *sp = NULL;       /* returned subraster */
    raster *rp = NULL;          /* new composite raster in sp */
    int base1 = sp1->baseline,  /*baseline for underlying subraster */
        height1 = (sp1->image)->height, /* height for underlying subraster */
        width1 = (sp1->image)->width,   /* width for underlying subraster */
        pixsz1 = (sp1->image)->pixsz,   /* pixsz for underlying subraster */
        base2 = sp2->baseline,  /*baseline for overlaid subraster */
        height2 = (sp2->image)->height, /* height for overlaid subraster */
        width2 = (sp2->image)->width,   /* width for overlaid subraster */
        pixsz2 = (sp2->image)->pixsz;   /* pixsz for overlaid subraster */
    int height = max2(height1, height2),        /*composite height if sp2 centered */
        base = base1 + (height - height1) / 2,  /* and composite baseline */
        tlc2 = (height - height2) / 2,  /* top-left corner for overlay */
        width = 0, pixsz = 0;   /* other params for composite */
    int lift1 = rastlift1,      /* vertical \raisebox lift for sp1 */
        lift2 = rastlift;       /* vertical \raisebox lift for sp2 */
/* -------------------------------------------------------------------------
Initialization
-------------------------------------------------------------------------- */
/* --- determine height, width and baseline of composite raster --- */
    switch (isalign) {
    default:
    case 0:                    /* centered, baselines not aligned */
        height = max2(height1, height2);        /* max height */
        base = base1 + (height - height1) / 2;  /* baseline for sp1 */
        break;
    case 1:                    /* baselines of sp1,sp2 aligned */
        height = max2(base1 + 1, base2 + 1)     /* max height above baseline */
            +max2(height1 - base1 - 1, height2 - base2 - 1);    /*+max descending below */
        base = max2(base1, base2);      /* max space above baseline */
        break;
    case 2:                    /* centered +/- \raisebox lifts */
        base1 -= lift1;
        base2 -= lift2;         /* reset to unlifted images */
        /* --- start with default for centered, unlifted images --- */
        height2 += 2 * absval(lift2);   /* "virtual" height of overlay */
        height = max2(height1, height2);        /* max height */
        base = base1 + (height - height1) / 2;  /* baseline for sp1 */
        tlc2 = (height - height2) / 2   /* top-left corner for overlay */
            + (lift2 >= 0 ? 0 : 2 * absval(lift2));     /* "reflect" overlay below base */
        break;
    }
    width = max2(width1, width2 + abs(offset2));        /* max width */
    pixsz = max2(pixsz1, pixsz2);       /* bitmap,bytemap becomes bytemap */
/* -------------------------------------------------------------------------
allocate concatted composite subraster
-------------------------------------------------------------------------- */
/* --- allocate returned subraster (and then initialize it) --- */
    if ((sp = new_subraster(width, height, pixsz))      /* allocate new subraster */
        == (subraster *) NULL)
        goto end_of_job;        /* failed, so quit */
/* --- initialize subraster parameters --- */
    sp->type = IMAGERASTER;     /* image */
    sp->baseline = base;        /* composite baseline */
    sp->size = sp1->size;       /* underlying char is sp1 */
    if (isalign == 2)
        sp->baseline += lift1;  /* adjust baseline */
/* --- extract raster from subraster --- */
    rp = sp->image;             /* raster allocated in subraster */
/* -------------------------------------------------------------------------
overlay sp1 and sp2 in new composite raster
-------------------------------------------------------------------------- */
    switch (isalign) {
    default:
    case 0:                    /* centered, baselines not aligned */
        rastput(rp, sp1->image, base - base1, (width - width1) / 2, 1); /*underlying */
        rastput(rp, sp2->image, (height - height2) / 2, /*overlaid */
                (width - width2) / 2 + offset2, 0);
        break;
    case 1:                    /* baselines of sp1,sp2 aligned */
        rastput(rp, sp1->image, base - base1, (width - width1) / 2, 1); /*underlying */
        rastput(rp, sp2->image, base - base2,   /*overlaid */
                (width - width2) / 2 + offset2, 0);
        break;
    case 2:
        if (1) {                /* centered +/- \raisebox lifts */
            rastput(rp, sp1->image, base - base1, (width - width1) / 2, 1);
            rastput(rp, sp2->image, tlc2, (width - width2) / 2 + offset2,
                    0);
        }
        break;
    }
/* -------------------------------------------------------------------------
free input if requested
-------------------------------------------------------------------------- */
    if (isfree > 0) {           /* caller wants input freed */
        if (isfree == 1 || isfree > 2) {
            delete_subraster(sp1);
        }
        if (isfree >= 2) {
            delete_subraster(sp2);
        }
    }
    /* and/or sp2 */
  end_of_job:
    return sp;
}

/* ==========================================================================
 * Function:    rastcat ( sp1, sp2, isfree )
 * Purpose: "Concatanates" subrasters sp1||sp2, leaving both unchanged
 *      and returning a newly-allocated subraster.
 *      Frees/deletes input sp1 and/or sp2 depending on value
 *      of isfree (0=none, 1=sp1, 2=sp2, 3=both).
 * --------------------------------------------------------------------------
 * Arguments:   sp1 (I)     subraster *  to left-hand subraster
 *      sp2 (I)     subraster *  to right-hand subraster
 *      isfree (I)  int containing 1=free sp1 before return,
 *              2=free sp2, 3=free both, 0=free none.
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) pointer to constructed subraster sp1||sp2
 *              or  NULL for any error
 * --------------------------------------------------------------------------
 * Notes:
 * ======================================================================= */
subraster *rastcat(subraster * sp1, subraster * sp2, int isfree)
{
    subraster *sp = NULL;       /* returned subraster */
    raster *rp = NULL;          /* new concatted raster */
    int base1 = sp1->baseline,  /*baseline for left-hand subraster */
        height1 = (sp1->image)->height, /* height for left-hand subraster */
        width1 = (sp1->image)->width,   /* width for left-hand subraster */
        pixsz1 = (sp1->image)->pixsz,   /* pixsz for left-hand subraster */
        type1 = sp1->type,      /* image type for left-hand */
        base2 = sp2->baseline,  /*baseline for right-hand subraster */
        height2 = (sp2->image)->height, /* height for right-hand subraster */
        width2 = (sp2->image)->width,   /* width for right-hand subraster */
        pixsz2 = (sp2->image)->pixsz,   /* pixsz for right-hand subraster */
        type2 = sp2->type;      /* image type for right-hand */
    int height = 0, width = 0, pixsz = 0, base = 0;     /*concatted sp1||sp2 composite */
    int issmash = (smashmargin != 0 ? 1 : 0),   /* true to "squash" sp1||sp2 */
        isopaque = (issmash ? 0 : 1),   /* not oppaque if smashing */
        isblank = 0, nsmash = 0,        /* #cols to smash */
        oldsmashmargin = smashmargin,   /* save original smashmargin */
        oldblanksymspace = blanksymspace,       /* save original blanksymspace */
        oldnocatspace = isnocatspace;   /* save original isnocatspace */
    mathchardef *symdef1 = sp1->symdef, /*mathchardef of last left-hand char */
        *symdef2 = sp2->symdef; /* mathchardef of right-hand char */
    int class1 = (symdef1 == NULL ? ORDINARY : symdef1->class), /* symdef->class */
        class2 = (symdef2 == NULL ? ORDINARY : symdef2->class), /* or default */
        smash1 = (symdef1 != NULL) && (class1 == ORDINARY || class1 == VARIABLE || class1 == OPENING || class1 == CLOSING || class1 == PUNCTION), smash2 = (symdef2 != NULL) && (class2 == ORDINARY || class2 == VARIABLE || class2 == OPENING || class2 == CLOSING || class2 == PUNCTION), space = fontsize / 2 + 1;   /* #cols between sp1 and sp2 */
    int isfrac = (type1 == FRACRASTER && class2 == PUNCTION);   /* sp1 is a \frac and sp2 is punctuation */
/* -------------------------------------------------------------------------
Initialization
-------------------------------------------------------------------------- */
/* --- determine inter-character space from character class --- */
    if (!isstring)
        space = max2(2, (symspace[class1][class2] + fontsize - 3));     /* space */
    else
        space = 1;              /* space for ascii string */
    if (isnocatspace > 0) {     /* spacing explicitly turned off */
        space = 0;              /* reset space */
        isnocatspace--;
    }                           /* and decrement isnocatspace flag */
    if (0 && sp1->type == BLANKSIGNAL)
        space = 0;              /*implicitly turn off spacing */
    if (sp1->type == BLANKSIGNAL && sp2->type == BLANKSIGNAL)   /* both blank */
        space = 0;              /* no extra space between spaces */
    if (sp2->type != BLANKSIGNAL)       /* not a blank space signal */
        if (blanksymspace != 0) {       /* and we have a space adjustment */
            space = max2(0, space + blanksymspace);     /* adjust as much as possible */
            blanksymspace = 0;
        }                       /* and reset adjustment */
    if (msgfp != NULL && msglevel >= 999) {     /* display space results */
        fprintf(msgfp,
                "rastcat> space=%d, blanksymspace=%d, isnocatspace=%d\n",
                space, oldblanksymspace, oldnocatspace);
        fflush(msgfp);
    }
/* --- determine smash --- */
    if (!isstring && !isfrac) {  /* don't smash strings or \frac's */
        if (issmash) {          /* raster smash wanted */
            int maxsmash = rastsmash(sp1, sp2), /* calculate max smash space */
                margin = smashmargin;   /* init margin without delta */
            if ((1 && smash1 && smash2) /* concatanating two chars */
                ||(1 && type1 != IMAGERASTER && type2 != IMAGERASTER
                   && type1 != FRACRASTER && type2 != FRACRASTER))
                /*maxsmash = 0; *//* turn off smash */
                margin = max2(space - 1, 0);    /* force small smashmargin */
            else /* adjust for delta if images */ if (issmashdelta)     /* smashmargin is a delta value */
                margin += fontsize;     /* add displaystyle base to margin */
            if (maxsmash == blanksignal)        /* sp2 is intentional blank */
                isblank = 1;    /* set blank flag signal */
            else /* see how much extra space we have */ if (maxsmash > margin)  /* enough space for adjustment */
                nsmash = maxsmash - margin;     /* make adjustment */
            if (msgfp != NULL && msglevel >= 99) {      /* display smash results */
                fprintf(msgfp,
                        "rastcat> maxsmash=%d, margin=%d, nsmash=%d\n",
                        maxsmash, margin, nsmash);
                fprintf(msgfp, "rastcat> type1=%d,2=%d, class1=%d,2=%d\n",
                        type1, type2, (symdef1 == NULL ? -999 : class1),
                        (symdef2 == NULL ? -999 : class2));
                fflush(msgfp);
            }
        }
    }

    /* --- determine height, width and baseline of composite raster --- */
    if (!isstring) {
        height = max2(base1 + 1, base2 + 1)     /* max height above baseline */
            +max2(height1 - base1 - 1, height2 - base2 - 1);    /*+ max descending below */
        width = width1 + width2 + space - nsmash;       /*add widths and space-smash */
        width = max3(width, width1, width2);
    } else {                      /* ascii string */
        height = 1;             /* default */
        width = width1 + width2 + space - 1;
    }                           /* no need for two nulls */
    pixsz = max2(pixsz1, pixsz2);       /* bitmap||bytemap becomes bytemap */
    base = max2(base1, base2);  /* max space above baseline */
    if (msgfp != NULL && msglevel >= 9999) {    /* display components */
        fprintf(msgfp,
                "rastcat> Left-hand ht,width,pixsz,base = %d,%d,%d,%d\n",
                height1, width1, pixsz1, base1);
        type_raster(sp1->image, msgfp); /* display left-hand raster */
        fprintf(msgfp,
                "rastcat> Right-hand ht,width,pixsz,base = %d,%d,%d,%d\n",
                height2, width2, pixsz2, base2);
        type_raster(sp2->image, msgfp); /* display right-hand raster */
        fprintf(msgfp,
                "rastcat> Composite ht,width,smash,pixsz,base = %d,%d,%d,%d,%d\n",
                height, width, nsmash, pixsz, base);
        fflush(msgfp);
    }

    /* -------------------------------------------------------------------------
       allocate concatted composite subraster
       -------------------------------------------------------------------------- */
    /* --- allocate returned subraster (and then initialize it) --- */
    if (msgfp != NULL && msglevel >= 9999) {
        fprintf(msgfp, "rastcat> calling new_subraster(%d,%d,%d)\n",
                width, height, pixsz);
        fflush(msgfp);
    }
    if ((sp = new_subraster(width, height, pixsz)) == NULL) {   /* failed */
        if (msgfp != NULL && msglevel >= 1) {   /* report failure */
            fprintf(msgfp, "rastcat> new_subraster(%d,%d,%d) failed\n",
                    width, height, pixsz);
            fflush(msgfp);
        }
        goto end_of_job;
    }

    /* failed, so quit */
    /* --- initialize subraster parameters --- */
    /* sp->type = (!isstring?STRINGRASTER:ASCIISTRING); */
    /*concatted string */
    if (!isstring) {
        sp->type =              /*type2; *//*(type1==type2?type2:IMAGERASTER); */
            (type2 != CHARASTER ? type2 :
             (type1 != CHARASTER && type1 != BLANKSIGNAL
              && type1 != FRACRASTER ? type1 : IMAGERASTER));
    } else {
        sp->type = ASCIISTRING; /* concatted ascii string */
    }
    sp->symdef = symdef2;       /* rightmost char is sp2 */
    sp->baseline = base;        /* composite baseline */
    sp->size = sp2->size;       /* rightmost char is sp2 */
    if (isblank) {              /* need to propagate blanksignal */
        sp->type = blanksignal; /* may not be completely safe??? */
    }
/* --- extract raster from subraster --- */
    rp = sp->image;             /* raster allocated in subraster */
/* -------------------------------------------------------------------------
overlay sp1 and sp2 in new composite raster
-------------------------------------------------------------------------- */
    if (msgfp != NULL && msglevel >= 9999) {
        fprintf(msgfp,
                "rastcat> calling rastput() to concatanate left||right\n");
        fflush(msgfp);
    }                           /* flush msgfp buffer */
    if (!isstring) {
        rastput(rp, sp1->image, base - base1,   /* overlay left-hand */
                max2(0, nsmash - width1), 1);   /* plus any residual smash space */
    } else {
        memcpy(rp->pixmap, (sp1->image)->pixmap, width1 - 1);   /*init left string */
    }
    if (msgfp != NULL && msglevel >= 9999) {
        type_raster(sp->image, msgfp);  /* display composite raster */
        fflush(msgfp);
    }                           /* flush msgfp buffer */
    if (!isstring) {
        int fracbase = (isfrac ?        /* baseline for punc after \frac */
                        max2(fraccenterline, base2) : base);    /*adjust baseline or use original */
        rastput(rp, sp2->image, fracbase - base2,       /* overlay right-hand */
                max2(0, width1 + space - nsmash), isopaque);    /* minus any smashed space */
        if (1 && type1 == FRACRASTER    /* we're done with \frac image */
            && type2 != FRACRASTER)     /* unless we have \frac\frac */
            fraccenterline = NOVALUE;   /* so reset centerline signal */
        if (fraccenterline != NOVALUE)  /* sp2 is a fraction */
            fraccenterline += (base - base2);
    } else {
        strcpy((char *) (rp->pixmap) + width1 - 1 + space,
               (char *) ((sp2->image)->pixmap));
        ((char *) (rp->pixmap))[width1 + width2 + space - 2] = '\0';
    }
    if (msgfp != NULL && msglevel >= 9999) {
        type_raster(sp->image, msgfp);  /* display composite raster */
        fflush(msgfp);
    }
    /* flush msgfp buffer */
    /* -------------------------------------------------------------------------
       free input if requested
       -------------------------------------------------------------------------- */
    if (isfree > 0) {           /* caller wants input freed */
        if (isfree == 1 || isfree > 2) {
            delete_subraster(sp1);
        }
        if (isfree >= 2) {
            delete_subraster(sp2);
        }
    }
    /* and/or sp2 */
  end_of_job:
    smashmargin = oldsmashmargin;
    return sp;
}

/* ==========================================================================
 * Function:    rastack ( sp1, sp2, base, space, iscenter, isfree )
 * Purpose: Stack subrasters sp2 atop sp1, leaving both unchanged
 *      and returning a newly-allocated subraster,
 *      whose baseline is sp1's if base=1, or sp2's if base=2.
 *      Frees/deletes input sp1 and/or sp2 depending on value
 *      of isfree (0=none, 1=sp1, 2=sp2, 3=both).
 * --------------------------------------------------------------------------
 * Arguments:   sp1 (I)     subraster *  to lower subraster
 *      sp2 (I)     subraster *  to upper subraster
 *      base (I)    int containing 1 if sp1 is baseline,
 *              or 2 if sp2 is baseline.
 *      space (I)   int containing #rows blank space inserted
 *              between sp1's image and sp2's image.
 *      iscenter (I)    int containing 1 to center both sp1 and sp2
 *              in stacked array, 0 to left-justify both
 *      isfree (I)  int containing 1=free sp1 before return,
 *              2=free sp2, 3=free both, 0=free none.
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) pointer to constructed subraster sp2 atop sp1
 *              or  NULL for any error
 * --------------------------------------------------------------------------
 * Notes:
 * ======================================================================= */
subraster *rastack(subraster * sp1, subraster * sp2, int base, int space,
                   int iscenter, int isfree)
{
    subraster *sp = NULL;       /* returned subraster */
    raster *rp = NULL;          /* new stacked raster in sp */
    int base1 = sp1->baseline,  /* baseline for lower subraster */
        height1 = (sp1->image)->height, /* height for lower subraster */
        width1 = (sp1->image)->width,   /* width for lower subraster */
        pixsz1 = (sp1->image)->pixsz,   /* pixsz for lower subraster */
        base2 = sp2->baseline,  /* baseline for upper subraster */
        height2 = (sp2->image)->height, /* height for upper subraster */
        width2 = (sp2->image)->width,   /* width for upper subraster */
        pixsz2 = (sp2->image)->pixsz;   /* pixsz for upper subraster */
    int height = 0, width = 0, pixsz = 0, baseline = 0; /*for stacked sp2 atop sp1 */
    mathchardef *symdef1 = sp1->symdef, /* mathchardef of right lower char */
        *symdef2 = sp2->symdef; /* mathchardef of right upper char */
/* -------------------------------------------------------------------------
Initialization
-------------------------------------------------------------------------- */
/* --- determine height, width and baseline of composite raster --- */
    height = height1 + space + height2; /* sum of heights plus space */
    width = max2(width1, width2);       /* max width is overall width */
    pixsz = max2(pixsz1, pixsz2);       /* bitmap||bytemap becomes bytemap */
    baseline =
        (base == 1 ? height2 + space + base1 : (base == 2 ? base2 : 0));
/* -------------------------------------------------------------------------
allocate stacked composite subraster (with embedded raster)
-------------------------------------------------------------------------- */
/* --- allocate returned subraster (and then initialize it) --- */
    if ((sp = new_subraster(width, height, pixsz)) == NULL) {
        goto end_of_job;        /* failed, so quit */
    }
/* --- initialize subraster parameters --- */
    sp->type = IMAGERASTER;     /* stacked rasters */
    sp->symdef = (base == 1 ? symdef1 : (base == 2 ? symdef2 : NULL));  /* symdef */
    sp->baseline = baseline;    /* composite baseline */
    sp->size = (base == 1 ? sp1->size : (base == 2 ? sp2->size : NORMALSIZE));  /*size */
/* --- extract raster from subraster --- */
    rp = sp->image;             /* raster embedded in subraster */
/* -------------------------------------------------------------------------
overlay sp1 and sp2 in new composite raster
-------------------------------------------------------------------------- */
    if (iscenter == 1) {        /* center both sp1 and sp2 */
        rastput(rp, sp2->image, 0, (width - width2) / 2, 1);    /* overlay upper */
        rastput(rp, sp1->image, height2 + space, (width - width1) / 2, 1);
    } else {
        /*lower */
        /* left-justify both sp1 and sp2 */
        rastput(rp, sp2->image, 0, 0, 1);       /* overlay upper */
        rastput(rp, sp1->image, height2 + space, 0, 1);
    }
/* -------------------------------------------------------------------------
free input if requested
-------------------------------------------------------------------------- */
    if (isfree > 0) {           /* caller wants input freed */
        if (isfree == 1 || isfree > 2) {
            delete_subraster(sp1);
        }
        if (isfree >= 2) {
            delete_subraster(sp2);
        }
    }
    /* and/or sp2 */
  end_of_job:
    return sp;
}

/* ==========================================================================
 * Function:    rastile ( tiles, ntiles )
 * Purpose: Allocate and build up a composite raster
 *      from the ntiles components/characters supplied in tiles.
 * --------------------------------------------------------------------------
 * Arguments:   tiles (I)   subraster *  to array of subraster structs
 *              describing the components and their locations
 *      ntiles (I)  int containing number of subrasters in tiles[]
 * --------------------------------------------------------------------------
 * Returns: ( raster * )    ptr to composite raster,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o The top,left corner of a raster is row=0,col=0
 *      with row# increasing as you move down,
 *      and col# increasing as you move right.
 *      Metafont numbers rows with the baseline=0,
 *      so the top row is a positive number that
 *      decreases as you move down.
 *        o rastile() is no longer used.
 *      It was used by an earlier rasterize() algorithm,
 *      and I've left it in place should it be needed again.
 *      But recent changes haven't been tested/exercised.
 * ======================================================================= */
raster *rastile(subraster * tiles, int ntiles)
{
    raster *composite = NULL;   /*raster back to caller */
    int width = 0, height = 0, pixsz = 0,       /*width,height,pixsz of composite raster */
        toprow = 9999, rightcol = -999, /* extreme upper-right corner of tiles */
        botrow = -999, leftcol = 9999;  /* extreme lower-left corner of tiles */
    int itile;                  /* tiles[] index */
/* -------------------------------------------------------------------------
run through tiles[] to determine dimensions for composite raster
-------------------------------------------------------------------------- */
/* --- determine row and column bounds of composite raster --- */
    for (itile = 0; itile < ntiles; itile++) {
        subraster *tile = &tiles[itile];        /* ptr to current tile */
        /* --- upper-left corner of composite --- */
        toprow = min2(toprow, tile->toprow);
        leftcol = min2(leftcol, tile->leftcol);
        /* --- lower-right corner of composite --- */
        botrow = max2(botrow, tile->toprow + (tile->image)->height - 1);
        rightcol =
            max2(rightcol, tile->leftcol + (tile->image)->width - 1);
        /* --- pixsz of composite --- */
        pixsz = max2(pixsz, (tile->image)->pixsz);
    }
/* --- calculate width and height from bounds --- */
    width = rightcol - leftcol + 1;
    height = botrow - toprow + 1;
/* --- sanity check (quit if bad dimensions) --- */
    if (width < 1 || height < 1)
        goto end_of_job;
/* -------------------------------------------------------------------------
allocate composite raster, and embed tiles[] within it
-------------------------------------------------------------------------- */
/* --- allocate composite raster --- */
    if ((composite = new_raster(width, height, pixsz)) == NULL) {
        goto end_of_job;        /* and quit if failed */
    }
/* --- embed tiles[] in composite --- */
    for (itile = 0; itile < ntiles; itile++) {
        subraster *tile = &(tiles[itile]);      /* ptr to current tile */
        rastput(composite, tile->image, /* overlay tile image at... */
                tile->toprow - toprow, tile->leftcol - leftcol, 1);
    }                           /*upper-left corner */
  end_of_job:
    return composite;
}

/* ==========================================================================
 * Function:    rastsmash ( sp1, sp2 )
 * Purpose: When concatanating sp1||sp2, calculate #pixels
 *      we can "smash sp2 left"
 * --------------------------------------------------------------------------
 * Arguments:   sp1 (I)     subraster *  to left-hand raster
 *      sp2 (I)     subraster *  to right-hand raster
 * --------------------------------------------------------------------------
 * Returns: ( int )     max #pixels we can smash sp1||sp2,
 *              or "blanksignal" if sp2 intentionally blank,
 *              or 0 for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static int rastsmash(subraster * sp1, subraster * sp2)
{
    int nsmash = 0;             /* #pixels to smash sp1||sp2 */
    int base1 = sp1->baseline,  /*baseline for left-hand subraster */
        height1 = (sp1->image)->height, /* height for left-hand subraster */
        width1 = (sp1->image)->width,   /* width for left-hand subraster */
        base2 = sp2->baseline,  /*baseline for right-hand subraster */
        height2 = (sp2->image)->height, /* height for right-hand subraster */
        width2 = (sp2->image)->width;   /* width for right-hand subraster */
    int base = max2(base1, base2),      /* max ascenders - 1 above baseline */
        top1 = base - base1, top2 = base - base2,       /* top irow indexes for sp1, sp2 */
        bot1 = top1 + height1 - 1, bot2 = top2 + height2 - 1,   /* bot irow indexes */
        height = max2(bot1, bot2) + 1;  /* total height */
    int irow1 = 0, irow2 = 0, icol = 0; /* row,col indexes */
    int firstcol1[1025], nfirst1 = 0,   /* 1st sp1 col containing set pixel */
        firstcol2[1025], nfirst2 = 0;   /* 1st sp2 col containing set pixel */
    int smin = 9999, xmin = 9999 /* not used --, ymin = 9999 */ ;       /* min separation (s=x+y) */
/* -------------------------------------------------------------------------
find right edge of sp1 and left edge of sp2 (these will be abutting edges)
-------------------------------------------------------------------------- */
/* --- check args --- */
    if (isstring) {
        goto end_of_job;        /* ignore string rasters */
    }
    if (0 && istextmode) {
        goto end_of_job;        /* don't smash in text mode */
    }
    if (height > 1023) {
        goto end_of_job;        /* don't try to smash huge image */
    }
    if (sp2->type == blanksignal) {     /*blanksignal was propagated to us */
        goto end_of_job;        /* don't smash intentional blank */
    }
/* --- init firstcol1[], firstcol2[] --- */
    for (irow1 = 0; irow1 < height; irow1++) {  /* for each row */
        firstcol1[irow1] = firstcol2[irow1] = blanksignal;      /* signal empty rows */
    }
/* --- set firstcol2[] indicating left edge of sp2 --- */
    for (irow2 = top2; irow2 <= bot2; irow2++) {        /* for each row inside sp2 */
        for (icol = 0; icol < width2; icol++) { /* find first non-empty col in row */
            if (getpixel(sp2->image, irow2 - top2, icol) != 0) {        /* found a set pixel */
                firstcol2[irow2] = icol;        /* icol is #cols from left edge */
                nfirst2++;      /* bump #rows containing set pixels */
                break;
            }                   /* and go on to next row */
        }
    }
    if (nfirst2 < 1) {          /*right-hand sp2 is completely blank */
        nsmash = blanksignal;   /* signal intentional blanks */
        goto end_of_job;
    }
    /* don't smash intentional blanks */
    /* --- now check if preceding image in sp1 was an intentional blank --- */
    if (sp1->type == blanksignal) {     /*blanksignal was propagated to us */
        goto end_of_job;        /* don't smash intentional blank */
    }
/* --- set firstcol1[] indicating right edge of sp1 --- */
    for (irow1 = top1; irow1 <= bot1; irow1++) {        /* for each row inside sp1 */
        for (icol = width1 - 1; icol >= 0; icol--) {    /* find last non-empty col in row */
            if (getpixel(sp1->image, irow1 - top1, icol) != 0) {        /* found a set pixel */
                firstcol1[irow1] = (width1 - 1) - icol; /* save #cols from right edge */
                nfirst1++;      /* bump #rows containing set pixels */
                break;
            }                   /* and go on to next row */
        }
    }
    if (nfirst1 < 1) {          /*left-hand sp1 is completely blank */
        goto end_of_job;        /* don't smash intentional blanks */
    }
/* -------------------------------------------------------------------------
find minimum separation
-------------------------------------------------------------------------- */
    for (irow2 = top2; irow2 <= bot2; irow2++) {        /* check each row inside sp2 */
        int margin1, margin2 = firstcol2[irow2];        /* #cols to first set pixel */
        if (margin2 != blanksignal) {   /* irow2 not an empty/blank row */
            for (irow1 = max2(irow2 - smin, top1);; irow1++) {
                if (irow1 > min2(irow2 + smin, bot1)) {
                    break;      /* upper bound check */
                } else if ((margin1 = firstcol1[irow1]) != blanksignal) { /*have non-blank row */
                    int dx = (margin1 + margin2), dy = absval(irow2 - irow1), ds = dx + dy;     /* deltas */
                    if (ds >= smin) {
                        continue;       /* min unchanged */
                    }
                    if (dy > smashmargin && dx < xmin && smin < 9999) {
                        continue;       /* dy alone */
                    }
                    smin = ds;
                    xmin = dx;
                    /* not used -- ymin = dy */ ;
                }
            }
        }
        if (smin < 2) {
            goto end_of_job;
        }
    }
    /*nsmash = min2(xmin,width2); */
    /* permissible smash */
    nsmash = xmin;              /* permissible smash */
/* -------------------------------------------------------------------------
Back to caller with #pixels to smash sp1||sp2
-------------------------------------------------------------------------- */
  end_of_job:
    /* --- debugging output --- */
    if (msgfp != NULL && msglevel >= 99) {      /* display for debugging */
        fprintf(msgfp, "rastsmash> nsmash=%d, smashmargin=%d\n",
                nsmash, smashmargin);
        if (msglevel >= 999) {  /* also display rasters */
            fprintf(msgfp, "rastsmash>left-hand image...\n");
            if (sp1 != NULL)
                type_raster(sp1->image, msgfp); /* left image */
            fprintf(msgfp, "rastsmash>right-hand image...\n");
            if (sp2 != NULL)
                type_raster(sp2->image, msgfp);
        }                       /* right image */
        fflush(msgfp);
    }
    return nsmash;
}

/* ==========================================================================
 * Function:    rastsmashcheck ( term )
 * Purpose: Check an exponent term to see if its leading symbol
 *      would make smashing dangerous
 * --------------------------------------------------------------------------
 * Arguments:   term (I)    char *  to null-terminated string
 *              containing right-hand exponent term about to
 *              be smashed against existing left-hand.
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if it's okay to smash term, or
 *              0 if smash is dangerous.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
int rastsmashcheck(const char *term)
{
    int isokay = 0;             /* 1 to signal okay to caller */
    static char nosmashchars[64] = "-.,=";      /* don't smash these leading chars */
    static const char *nosmashstrs[64] = { "\\frac", NULL };    /* or leading strings */
    static const char *grayspace[64] = {
        "\\tiny", "\\small", "\\normalsize",
        "\\large", "\\Large", "\\LARGE", "\\huge", "\\Huge", NULL
    };
    const char *expression = term;      /* local ptr to beginning of expression */
    const char *token = NULL;   /* token = nosmashstrs[i] or grayspace[i] */
    int i;

/* -------------------------------------------------------------------------
see if smash check enabled
-------------------------------------------------------------------------- */
    if (smashcheck < 1) {       /* no smash checking wanted */
        if (smashcheck >= 0) {  /* -1 means check should always fail */
            isokay = 1;         /* otherwise (if 0), signal okay to smash */
        }
        goto end_of_job;
    }

    /* return to caller */
    /* -------------------------------------------------------------------------
       skip leading white and gray space
       -------------------------------------------------------------------------- */
    /* --- first check input --- */
    if (isempty(term)) {
        goto end_of_job;        /* no input so return 0 to caller */
    }
/* --- skip leading white space --- */
    skipwhite(term);            /* skip leading white space */
    if (*term == '\0') {
        goto end_of_job;        /* nothing but white space */
    }
/* --- skip leading gray space --- */
  skipgray:
    for (i = 0; (token = grayspace[i]) != NULL; i++) {  /* check each grayspace */
        if (strncmp(term, token, strlen(token)) == 0) { /* found grayspace */
            term += strlen(token);      /* skip past this grayspace token */
            skipwhite(term);    /* and skip any subsequent white space */
            if (*term == '\0') {        /* nothing left so quit */
                if (msgfp != NULL && msglevel >= 99)    /* display for debugging */
                    fprintf(msgfp,
                            "rastsmashcheck> only grayspace in %.32s\n",
                            expression);
                goto end_of_job;
            }
            goto skipgray;
        }
    }

    /* restart grayspace check from beginning */
    /* -------------------------------------------------------------------------
       check for leading no-smash single char
       -------------------------------------------------------------------------- */
    /* --- don't smash if term begins with a "nosmash" char --- */
    if ((token = strchr(nosmashchars, *term)) != NULL) {
        if (msgfp != NULL && msglevel >= 99)    /* display for debugging */
            fprintf(msgfp, "rastsmashcheck> char %.1s found in %.32s\n",
                    token, term);
        goto end_of_job;
    }
/* -------------------------------------------------------------------------
check for leading no-smash token
-------------------------------------------------------------------------- */
    for (i = 0; (token = nosmashstrs[i]) != NULL; i++) {        /* check each nosmashstr */
        if (strncmp(term, token, strlen(token)) == 0) { /* found a nosmashstr */
            if (msgfp != NULL && msglevel >= 99)        /* display for debugging */
                fprintf(msgfp, "rastsmashcheck> token %s found in %.32s\n",
                        token, term);
            goto end_of_job;
        }
    }
    /* so don't smash term */
    /* -------------------------------------------------------------------------
       back to caller
       -------------------------------------------------------------------------- */
    isokay = 1;                 /* no problem, so signal okay to smash */
  end_of_job:
    if (msgfp != NULL && msglevel >= 999)       /* display for debugging */
        fprintf(msgfp,
                "rastsmashcheck> returning isokay=%d for \"%.32s\"\n",
                isokay, (expression == NULL ? "<no input>" : expression));
    return isokay;
}

/* ==========================================================================
 * Function:    accent_subraster ( accent, width, height, direction, pixsz )
 * Purpose: Allocate a new subraster of width x height
 *      (or maybe different dimensions, depending on accent),
 *      and draw an accent (\hat or \vec or \etc) that fills it
 * --------------------------------------------------------------------------
 * Arguments:   accent (I)  int containing either HATACCENT or VECACCENT,
 *              etc, indicating the type of accent desired
 *      width (I)   int containing desired width of accent (#cols)
 *      height (I)  int containing desired height of accent(#rows)
 *      direction (I)   int containing desired direction of accent,
 *              +1=right, -1=left, 0=left/right
 *      pixsz (I)   int containing 1 for bitmap, 8 for bytemap
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to newly-allocated subraster with accent,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o Some accents have internally-determined dimensions,
 *      and caller should check dimensions in returned subraster
 * ======================================================================= */
static subraster *accent_subraster(int accent, int width, int height,
                                   int direction, int pixsz)
{
/* --- general info --- */
    raster *rp = NULL;          /*raster containing desired accent */
    subraster *sp = NULL;       /* subraster returning accent */
    int thickness = 1;          /* line thickness */
    /*int   pixval = (pixsz==1? 1 : (pixsz==8?255:(-1))); *//*black pixel value */
/* --- other working info --- */
    int col0, col1,             /* cols for line */
     row0, row1;                /* rows for line */
    subraster *accsp = NULL;    /*find suitable cmex10 symbol/accent */
/* --- info for under/overbraces, tildes, etc --- */
    char brace[16];             /*"{" for over, "}" for under, etc */
    int iswidthneg = 0;         /* set true if width<0 arg passed */
    int serifwidth = 0;         /* serif for surd */
    int isBig = 0;              /* true for ==>arrow, false for --> */
/* -------------------------------------------------------------------------
initialization
-------------------------------------------------------------------------- */
    if (width < 0) {
        width = (-width);
        iswidthneg = 1;
    }
    /* set neg width flag */
    /* -------------------------------------------------------------------------
       outer switch() traps accents that may change caller's height,width
       -------------------------------------------------------------------------- */
    switch (accent) {
    default:
        /* -----------------------------------------------------------------------
           inner switch() first allocates fixed-size raster for accents that don't
           ------------------------------------------------------------------------ */
        if ((rp = new_raster(width, height, pixsz))     /* allocate fixed-size raster */
            !=NULL)             /* and if we succeeded... */
            switch (accent) {   /* ...draw requested accent in it */
                /* --- unrecognized request --- */
            default:
                delete_raster(rp);      /* unrecognized accent requested */
                rp = NULL;
                break;          /* so free raster and signal error */
                /* --- bar request --- */
            case UNDERBARACCENT:
            case BARACCENT:
                thickness = 1;  /*height-1; *//* adjust thickness */
                if (accent == BARACCENT) {      /* bar is above expression */
                    row0 = row1 = max2(height - 3, 0);  /* row numbers for overbar */
                    line_raster(rp, row0, 0, row1, width - 1, thickness);
                } /*blanks at bot */
                else {          /* underbar is below expression */
                    row0 = row1 = min2(2, height - 1);  /* row numbers for underbar */
                    line_raster(rp, row0, 0, row1, width - 1, thickness);
                }               /*blanks at top */
                break;
                /* --- dot request --- */
            case DOTACCENT:
                thickness = height - 1; /* adjust thickness */
                /*line_raster(rp,0,width/2,1,(width/2)+1,thickness); *//*centered dot */
                rule_raster(rp, 0, (width + 1 - thickness) / 2, thickness, thickness, 3);       /*box */
                break;
                /* --- ddot request --- */
            case DDOTACCENT:
                thickness = height - 1; /* adjust thickness */
                col0 = max2((width + 1) / 3 - (thickness / 2) - 1, 0);  /* one-third of width */
                col1 = min2((2 * width + 1) / 3 - (thickness / 2) + 1, width - thickness);      /*2/3rds */
                if (col0 + thickness >= col1) { /* dots overlap */
                    col0 = max2(col0 - 1, 0);   /* try moving left dot more left */
                    col1 = min2(col1 + 1, width - thickness);
                }               /* and right dot right */
                if (col0 + thickness >= col1)   /* dots _still_ overlap */
                    thickness = max2(thickness - 1, 1); /* so try reducing thickness */
                /*line_raster(rp,0,col0,1,col0+1,thickness); *//*set dot at 1st third */
                /*line_raster(rp,0,col1,1,col1+1,thickness); *//*and another at 2nd */
                rule_raster(rp, 0, col0, thickness, thickness, 3);      /*box at 1st third */
                rule_raster(rp, 0, col1, thickness, thickness, 3);      /*box at 2nd third */
                break;
                /* --- hat request --- */
            case HATACCENT:
                thickness = 1;  /*(width<=12? 2 : 3); *//* adjust thickness */
                line_raster(rp, height - 1, 0, 0, width / 2, thickness);        /* / part of hat */
                line_raster(rp, 0, (width - 1) / 2, height - 1, width - 1, thickness);  /* \ part */
                break;
                /* --- sqrt request --- */
            case SQRTACCENT:
                serifwidth = SURDSERIFWIDTH(height);    /* leading serif on surd */
                col1 = SQRTWIDTH(height, (iswidthneg ? 1 : 2)) - 1;     /*right col of sqrt */
                /*col0 = (col1-serifwidth+2)/3; *//* midpoint col of sqrt */
                col0 = (col1 - serifwidth + 1) / 2;     /* midpoint col of sqrt */
                row0 = max2(1, ((height + 1) / 2) - 2); /* midpoint row of sqrt */
                row1 = height - 1;      /* bottom row of sqrt */
                /*line_raster(rp,row0,0,row1,col0,thickness); *//*descending portion */
                line_raster(rp, row0 + serifwidth, 0, row0, serifwidth,
                            thickness);
                line_raster(rp, row0, serifwidth, row1, col0, thickness);       /* descending */
                line_raster(rp, row1, col0, 0, col1, thickness);        /* ascending portion */
                line_raster(rp, 0, col1, 0, width - 1, thickness);      /*overbar of thickness 1 */
                break;
            }
        break;
        /* --- underbrace, overbrace request --- */
    case UNDERBRACE:
    case OVERBRACE:
        if (accent == UNDERBRACE) {
            strcpy(brace, "}"); /* start with } brace */
        }
        if (accent == OVERBRACE) {
            strcpy(brace, "{"); /* start with { brace */
        }
        if ((accsp = get_delim(brace, width, CMEX10)) != NULL) {
            /* found desired brace */
            rp = rastrot(accsp->image); /* rotate 90 degrees clockwise */
            delete_subraster(accsp);
        }                       /* and free subraster "envelope" */
        break;
        /* --- hat request --- */
    case HATACCENT:
        if (accent == HATACCENT) {
            strcpy(brace, "<"); /* start with < */
        }
        if ((accsp = get_delim(brace, width, CMEX10)) != NULL) {
            /* found desired brace */
            rp = rastrot(accsp->image); /* rotate 90 degrees clockwise */
            delete_subraster(accsp);
        }                       /* and free subraster "envelope" */
        break;
        /* --- vec request --- */
    case VECACCENT:
        height = 2 * (height / 2) + 1;  /* force height odd */
        if (absval(direction) >= 9) {   /* want ==> arrow rather than --> */
            isBig = 1;          /* signal "Big" arrow */
            direction -= 10;
        }                       /* reset direction = +1, -1, or 0 */
        if ((accsp = arrow_subraster(width, height, pixsz, direction, isBig))   /*arrow */
            !=NULL) {           /* succeeded */
            rp = accsp->image;  /* "extract" raster with bitmap */
            free((void *) accsp);
        }                       /* and free subraster "envelope" */
        break;
        /* --- tilde request --- */
    case TILDEACCENT:
        accsp = (width < 25 ? get_delim("\\sim", -width, CMSY10) : get_delim("~", -width, CMEX10));     /*width search for tilde */
        if (accsp != NULL)      /* found desired tilde */
            if ((sp = rastack(new_subraster(1, 1, pixsz), accsp, 1, 0, 1, 3))   /*space below */
                !=NULL) {       /* have tilde with space below it */
                rp = sp->image; /* "extract" raster with bitmap */
                free((void *) sp);      /* and free subraster "envelope" */
                leftsymdef = NULL;
            }                   /* so \tilde{x}^2 works properly */
        break;
    }
/* -------------------------------------------------------------------------
if we constructed accent raster okay, embed it in a subraster and return it
-------------------------------------------------------------------------- */
/* --- if all okay, allocate subraster to contain constructed raster --- */
    if (rp != NULL) {           /* accent raster constructed okay */
        if ((sp = new_subraster(0, 0, 0)) == NULL) {           /* and if we fail to allocate */
            delete_raster(rp);  /* free now-unneeded raster */
        } else {                  /* subraster allocated okay */
            /* --- init subraster parameters, embedding raster in it --- */
            sp->type = IMAGERASTER;     /* constructed image */
            sp->image = rp;     /* raster we just constructed */
            sp->size = (-1);    /* can't set font size here */
            sp->baseline = 0;
        }                       /* can't set baseline here */
    }
    return sp;
}

/* ==========================================================================
 * Function:    arrow_subraster ( width, height, pixsz, drctn, isBig )
 * Purpose: Allocate a raster/subraster and draw left/right arrow in it
 * --------------------------------------------------------------------------
 * Arguments:   width (I)   int containing number of cols for arrow
 *      height (I)  int containing number of rows for arrow
 *      pixsz (I)   int containing 1 for bitmap, 8 for bytemap
 *      drctn (I)   int containing +1 for right arrow,
 *              or -1 for left, 0 for leftright
 *      isBig (I)   int containing 1/true for \Long arrows,
 *              or false for \long arrows, i.e.,
 *              true for ===> or false for --->.
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to constructed left/right arrow
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *arrow_subraster(int width, int height, int pixsz,
                                  int drctn, int isBig)
{
    subraster *arrowsp = NULL;  /* allocate arrow subraster */
    int irow, midrow = height / 2;      /* index, midrow is arrowhead apex */
    int icol, thickness = (height > 15 ? 2 : 2);        /* arrowhead thickness and index */
    int pixval = (pixsz == 1 ? 1 : (pixsz == 8 ? 255 : (-1)));  /* black pixel value */
    int ipix;                   /* raster pixmap[] index */
    int npix = width * height;  /* #pixels malloced in pixmap[] */
/* -------------------------------------------------------------------------
allocate raster/subraster and draw arrow line
-------------------------------------------------------------------------- */
    if (height < 3) {
        height = 3;
        midrow = 1;
    }                           /* set minimum height */
    if ((arrowsp = new_subraster(width, height, pixsz)) == NULL) {
        /* allocate empty raster */
        goto end_of_job;        /* and quit if failed */
    }
    if (!isBig) {               /* single line */
        rule_raster(arrowsp->image, midrow, 0, width, 1, 0);    /*draw line across midrow */
    } else {
        int delta =
            (width > 6 ? (height > 15 ? 3 : (height > 7 ? 2 : 1)) : 1);
        rule_raster(arrowsp->image, midrow - delta, delta,
                    width - 2 * delta, 1, 0);
        rule_raster(arrowsp->image, midrow + delta, delta,
                    width - 2 * delta, 1, 0);
    }
/* -------------------------------------------------------------------------
construct arrowhead(s)
-------------------------------------------------------------------------- */
    for (irow = 0; irow < height; irow++) {     /* for each row of arrow */
        int delta = abs(irow - midrow); /*arrowhead offset for irow */
        /* --- right arrowhead --- */
        if (drctn >= 0)         /* right arrowhead wanted */
            for (icol = 0; icol < thickness; icol++) {  /* for arrowhead thickness */
                ipix = ((irow + 1) * width - 1) - delta - icol; /* rightmost-delta-icol */
                if (ipix >= 0) {        /* bounds check */
                    if (pixsz == 1) {    /* have a bitmap */
                        setlongbit((arrowsp->image)->pixmap, ipix);     /*turn on arrowhead bit */
                    } else if (pixsz == 8) {    /* check pixsz for bytemap */
                        arrowsp->image->pixmap[ipix] = pixval;
                    }
                }
            }
        /*set arrowhead byte */
        /* --- left arrowhead (same as right except for ipix calculation) --- */
        if (drctn <= 0) {        /* left arrowhead wanted */
            for (icol = 0; icol < thickness; icol++) {  /* for arrowhead thickness */
                ipix = irow * width + delta + icol;     /* leftmost bit+delta+icol */
                if (ipix < npix) {      /* bounds check */
                    if (pixsz == 1) {    /* have a bitmap */
                        setlongbit(arrowsp->image->pixmap, ipix);     /*turn on arrowhead bit */
                    } else if (pixsz == 8) {   /* check pixsz for bytemap */
                        arrowsp->image->pixmap[ipix] = pixval;
                    }
                }
            }
        }
    }
  end_of_job:
    return arrowsp;
}

/* ==========================================================================
 * Function:    uparrow_subraster ( width, height, pixsz, drctn, isBig )
 * Purpose: Allocate a raster/subraster and draw up/down arrow in it
 * --------------------------------------------------------------------------
 * Arguments:   width (I)   int containing number of cols for arrow
 *      height (I)  int containing number of rows for arrow
 *      pixsz (I)   int containing 1 for bitmap, 8 for bytemap
 *      drctn (I)   int containing +1 for up arrow,
 *              or -1 for down, or 0 for updown
 *      isBig (I)   int containing 1/true for \Long arrows,
 *              or false for \long arrows, i.e.,
 *              true for ===> or false for --->.
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to constructed up/down arrow
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
subraster *uparrow_subraster(int width, int height, int pixsz, int drctn,
                             int isBig)
{
    subraster *arrowsp = NULL;  /* allocate arrow subraster */
    int icol, midcol = width / 2;       /* index, midcol is arrowhead apex */
    int irow, thickness = (width > 15 ? 2 : 2); /* arrowhead thickness and index */
    int pixval = (pixsz == 1 ? 1 : (pixsz == 8 ? 255 : -1));    /* black pixel value */
    int ipix;                   /* raster pixmap[] index */
    int npix = width * height;  /* #pixels malloced in pixmap[] */
/* -------------------------------------------------------------------------
allocate raster/subraster and draw arrow line
-------------------------------------------------------------------------- */
    if (width < 3) {
        width = 3;
        midcol = 1;
    }                           /* set minimum width */
    if ((arrowsp = new_subraster(width, height, pixsz)) == NULL) {
        goto end_of_job;        /* and quit if failed */
    }
    if (!isBig) {               /* single line */
        rule_raster(arrowsp->image, 0, midcol, 1, height, 0);   /*draw line down midcol */
    } else {
        int delta =
            (height > 6 ? (width > 15 ? 3 : (width > 7 ? 2 : 1)) : 1);
        rule_raster(arrowsp->image, delta, midcol - delta, 1,
                    height - 2 * delta, 0);
        rule_raster(arrowsp->image, delta, midcol + delta, 1,
                    height - 2 * delta, 0);
    }
/* -------------------------------------------------------------------------
construct arrowhead(s)
-------------------------------------------------------------------------- */
    for (icol = 0; icol < width; icol++) {      /* for each col of arrow */
        int delta = abs(icol - midcol); /*arrowhead offset for icol */
        /* --- up arrowhead --- */
        if (drctn >= 0) {         /* up arrowhead wanted */
            for (irow = 0; irow < thickness; irow++) {  /* for arrowhead thickness */
                ipix = (irow + delta) * width + icol;   /* leftmost+icol */
                if (ipix < npix) {      /* bounds check */
                    if (pixsz == 1)     /* have a bitmap */
                        setlongbit((arrowsp->image)->pixmap, ipix);     /*turn on arrowhead bit */
                    else /* should have a bytemap */ if (pixsz == 8)    /* check pixsz for bytemap */
                        ((arrowsp->image)->pixmap)[ipix] = pixval;
                }
            }
        }
        /*set arrowhead byte */
        /* --- down arrowhead (same as up except for ipix calculation) --- */
        if (drctn <= 0) {         /* down arrowhead wanted */
            for (irow = 0; irow < thickness; irow++) {  /* for arrowhead thickness */
                ipix = (height - 1 - delta - irow) * width + icol;      /* leftmost + icol */
                if (ipix > 0) { /* bounds check */
                    if (pixsz == 1) {    /* have a bitmap */
                        setlongbit((arrowsp->image)->pixmap, ipix);     /*turn on arrowhead bit */
                    } else if (pixsz == 8) {    /* check pixsz for bytemap */
                        arrowsp->image->pixmap[ipix] = pixval;
                    }
                }
            }
        }
    }
  end_of_job:
    return arrowsp;
}

/* ==========================================================================
 * Function:    rule_raster ( rp, top, left, width, height, type )
 * Purpose: Draw a solid or dashed line (or box) in existing raster rp,
 *      starting at top,left with dimensions width,height.
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster in which rule
 *              will be drawn
 *      top (I)     int containing row at which top-left corner
 *              of rule starts (0 is topmost)
 *      left (I)    int containing col at which top-left corner
 *              of rule starts (0 is leftmost)
 *      width (I)   int containing number of cols for rule
 *      height (I)  int containing number of rows for rule
 *      type (I)    int containing 0 for solid rule,
 *              1 for horizontal dashes, 2 for vertical
 *              3 for solid rule with corners removed (bevel)
 *              4 for strut (nothing drawn)
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if rule drawn okay,
 *              or 0 for any error.
 * --------------------------------------------------------------------------
 * Notes:     o Rule line is implicitly "horizontal" or "vertical" depending
 *      on relative width,height dimensions.  It's a box if they're
 *      more or less comparable.
 * ======================================================================= */
static int rule_raster(raster * rp, int top, int left, int width,
                       int height, int type)
{
    int irow = 0, icol = 0;     /* indexes over rp raster */
    int ipix = 0,               /* raster pixmap[] index */
        npix = rp->width * rp->height;  /* #pixels malloced in rp->pixmap[] */
    int isfatal = 0;            /* true to abend on out-of-bounds error */
    int hdash = 1, vdash = 2,   /* type for horizontal, vertical dashes */
        bevel = 99 /*3 */ , strut = 4;  /* type for bevel (turned off), strut */
    int dashlen = 3, spacelen = 2,      /* #pixels for dash followed by space */
        isdraw = 1;             /* true when drawing dash (init for solid) */
/* -------------------------------------------------------------------------
Check args
-------------------------------------------------------------------------- */
    if (rp == (raster *) NULL) {        /* no raster arg supplied */
        if (workingbox != (subraster *) NULL)   /* see if we have a workingbox */
            rp = workingbox->image;     /* use workingbox if possible */
        else
            return (0);
    }                           /* otherwise signal error to caller */
    if (type == bevel)          /* remove corners of solid box */
        if (width < 3 || height < 3)
            type = 0;           /* too small to remove corners */
/* -------------------------------------------------------------------------
Fill line/box
-------------------------------------------------------------------------- */
    if (width > 0) {            /* zero width implies strut */
        for (irow = top; irow < top + height; irow++) { /* for each scan line */
            if (type == strut) {
                isdraw = 0;     /* draw nothing for strut */
            }
            if (type == vdash) {        /*set isdraw for vert dash */
                isdraw = (((irow - top) % (dashlen + spacelen)) < dashlen);
            }
            ipix = irow * rp->width + left - 1; /*first pixel preceding icol */
            for (icol = left; icol < left + width; icol++) {    /* each pixel in scan line */
                if (type == bevel) {    /* remove corners of box */
                    if ((irow == top && icol == left)   /* top-left corner */
                        ||(irow == top && icol >= left + width - 1)     /* top-right corner */
                        ||(irow >= top + height - 1 && icol == left)    /* bottom-left corner */
                        ||(irow >= top + height - 1 && icol >= left + width - 1)) {     /* bottom-right */
                        isdraw = 0;
                    } else {
                        isdraw = 1;
                    }
                }               /*set isdraw to skip corner */
                if (type == hdash) {    /*set isdraw for horiz dash */
                    isdraw =
                        (((icol - left) % (dashlen + spacelen)) < dashlen);
                }
                if (++ipix >= npix) {   /* bounds check failed */
                    if (isfatal) {
                        goto end_of_job;        /* abort if error is fatal */
                    } else {
                        break;  /*or just go on to next row */
                    }
                } else if (isdraw) {
                    /* ibit is within rp bounds and we're drawing this bit */
                    if (rp->pixsz == 1) {       /* have a bitmap */
                        setlongbit(rp->pixmap, ipix);   /* so turn on bit in line */
                    } else if (rp->pixsz == 8)  /* should have a bytemap, check pixsz for bytemap */
                        ((unsigned char *) (rp->pixmap))[ipix] = 255;
                }               /* set black byte */
            }
        }
    }
  end_of_job:
    return (isfatal ? (ipix < npix ? 1 : 0) : 1);
}

/* ==========================================================================
 * Function:    line_raster ( rp,  row0, col0,  row1, col1,  thickness )
 * Purpose: Draw a line from row0,col0 to row1,col1 of thickness
 *      in existing raster rp.
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster in which a line
 *              will be drawn
 *      row0 (I)    int containing row at which
 *              line will start (0 is topmost)
 *      col0 (I)    int containing col at which
 *              line will start (0 is leftmost)
 *      row1 (I)    int containing row at which
 *              line will end (rp->height-1 is bottom-most)
 *      col1 (I)    int containing col at which
 *              line will end (rp->width-1 is rightmost)
 *      thickness (I)   int containing number of pixels/bits
 *              thick the line will be
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if line drawn okay,
 *              or 0 for any error.
 * --------------------------------------------------------------------------
 * Notes:     o if row0==row1, a horizontal line is drawn
 *      between col0 and col1, with row0(==row1) the top row
 *      and row0+(thickness-1) the bottom row
 *        o if col0==col1, a vertical bar is drawn
 *      between row0 and row1, with col0(==col1) the left col
 *      and col0+(thickness-1) the right col
 *        o if both the above, you get a square thickness x thickness
 *      whose top-left corner is row0,col0.
 * ======================================================================= */
int line_raster(raster * rp, int row0, int col0, int row1, int col1,
                int thickness)
{
    int irow = 0, icol = 0;     /* indexes over rp raster */
    int locol = col0, hicol = col1;     /* col limits at irow */
    int lorow = row0, hirow = row1;     /* row limits at icol */
    int width = rp->width, height = rp->height; /* dimensions of input raster */
    int ipix = 0;               /* raster pixmap[] index */
    int npix = width * height;  /* #pixels malloced in rp->pixmap[] */
    int isfatal = 0;            /* true to abend on out-of-bounds error */
    int isline = (row1 == row0), isbar = (col1 == col0);        /*true if slope a=0,\infty */
    double dy = row1 - row0;    /* + (row1>=row0? +1.0 : -1.0) *//* delta-x */
    double dx = col1 - col0;    /* + (col1>=col0? +1.0 : -1.0) *//* delta-y */
    double a = (isbar || isline ? 0.0 : dy / dx);       /* slope = tan(theta) = dy/dx */
    double xcol = 0.0, xrow = 0.0;      /* calculated col at irow, or row at icol */
    double ar = ASPECTRATIO;    /* aspect ratio width/height of one pixel */
    /*#pixels per row to get sloped line thcknss */
    double xwidth =
        (isline ? 0.0 : ((double) thickness) *
         sqrt((dx * dx) + (dy * dy * ar * ar)) / fabs(dy * ar));
    double xheight = 1.0;
    int isrecurse = 1;          /* true to draw line recursively */
/* -------------------------------------------------------------------------
Check args
-------------------------------------------------------------------------- */
    if (rp == NULL) {           /* no raster arg supplied */
        if (workingbox != NULL) {       /* see if we have a workingbox */
            rp = workingbox->image;     /* use workingbox if possible */
        } else {
            return 0;
        }
    }
    /* otherwise signal error to caller */
    /* -------------------------------------------------------------------------
       Initialization
       -------------------------------------------------------------------------- */
    if (msgfp != NULL && msglevel >= 29) {      /* debugging */
        fprintf(msgfp,
                "line_raster> row,col0=%d,%d row,col1=%d,%d, thickness=%d\n"
                "\t dy,dx=%3.1f,%3.1f, a=%4.3f, xwidth=%4.3f\n", row0,
                col0, row1, col1, thickness, dy, dx, a, xwidth);
        fflush(msgfp);
    }
/* --- check for recursive line drawing --- */
    if (isrecurse) {            /* drawing lines recursively */
        for (irow = 0; irow < thickness; irow++) {      /* each line 1 pixel thick */
            double xrow0 = (double) row0, xcol0 = (double) col0,
                xrow1 = (double) row1, xcol1 = (double) col1;
            if (isline)
                xrow0 = xrow1 = (double) (row0 + irow);
            else if (isbar)
                xcol0 = xcol1 = (double) (col0 + irow);
            if (xrow0 > (-0.001) && xcol0 > (-0.001)    /*check line inside raster */
                &&xrow1 < ((double) (height - 1) + 0.001)
                && xcol1 < ((double) (width - 1) + 0.001))
                line_recurse(rp, xrow0, xcol0, xrow1, xcol1, thickness);
        }
        return 1;
    }
/* --- set params for horizontal line or vertical bar --- */
    if (isline) {               /*interpret row as top row */
        row1 = row0 + (thickness - 1);  /* set bottom row for line */
    }
    if (0 && isbar) {           /*interpret col as left col */
        hicol = col0 + (thickness - 1); /* set right col for bar */
    }
/* -------------------------------------------------------------------------
draw line one row at a time
-------------------------------------------------------------------------- */
    for (irow = min2(row0, row1); irow <= max2(row0, row1); irow++) {   /*each scan line */
        if (!isbar && !isline) {        /* neither vert nor horiz */
            xcol = col0 + ((double) (irow - row0)) / a; /* "middle" col in irow */
            locol = max2((int) (xcol - 0.5 * (xwidth - 1.0)), 0);       /* leftmost col */
            hicol =
                min2((int) (xcol + 0.5 * (xwidth - 0.0)),
                     max2(col0, col1));
        }                       /*right */
        if (msgfp != NULL && msglevel >= 29)    /* debugging */
            fprintf(msgfp, "\t irow=%d, xcol=%4.2f, lo,hicol=%d,%d\n",
                    irow, xcol, locol, hicol);
        ipix = irow * rp->width + min2(locol, hicol) - 1;       /*first pix preceding icol */
        for (icol = min2(locol, hicol); icol <= max2(locol, hicol); icol++) {   /*each pix */
            if (++ipix >= npix) {       /* bounds check failed */
                if (isfatal) {
                    goto end_of_job;    /* abort if error is fatal */
                } else {
                    break;      /*or just go on to next row */
                }
            } else if (rp->pixsz == 1) {
                /* turn on pixel in line */
                /* have a pixel bitmap */
                setlongbit(rp->pixmap, ipix);   /* so turn on bit in line */
            } else if (rp->pixsz == 8) {
                /* should have a bytemap */
                /* check pixsz for bytemap */
                ((unsigned char *) (rp->pixmap))[ipix] = 255;   /* set black byte */
            }
        }
    }
/* -------------------------------------------------------------------------
now _redraw_ line one col at a time to avoid "gaps"
-------------------------------------------------------------------------- */
    if (1) {
        for (icol = min2(col0, col1); icol <= max2(col0, col1); icol++) {       /*each scan line */
            if (!isbar && !isline) {    /* neither vert nor horiz */
                xrow = row0 + ((double) (icol - col0)) * a;     /* "middle" row in icol */
                lorow = max2((int) (xrow - 0.5 * (xheight - 1.0)), 0);  /* topmost row */
                hirow =
                    min2((int) (xrow + 0.5 * (xheight - 0.0)),
                         max2(row0, row1));
            }                   /*bot */
            if (msgfp != NULL && msglevel >= 29)        /* debugging */
                fprintf(msgfp, "\t icol=%d, xrow=%4.2f, lo,hirow=%d,%d\n",
                        icol, xrow, lorow, hirow);
            ipix = irow * rp->width + min2(locol, hicol) - 1;   /*first pix preceding icol */
            for (irow = min2(lorow, hirow); irow <= max2(lorow, hirow); irow++) /*each pix */
                if (irow < 0 || irow >= rp->height || icol < 0 || icol >= rp->width)    /* bounds check */
                    if (isfatal)
                        goto end_of_job;        /* abort if error is fatal */
                    else
                        continue;       /*or just go on to next row */
                else
                    setpixel(rp, irow, icol, 255);      /* set pixel at irow,icol */
        }
    }
  end_of_job:
    return (isfatal ? (ipix < npix ? 1 : 0) : 1);
}

/* ==========================================================================
 * Function:    line_recurse ( rp,  row0, col0,  row1, col1,  thickness )
 * Purpose: Draw a line from row0,col0 to row1,col1 of thickness
 *      in existing raster rp.
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster in which a line
 *              will be drawn
 *      row0 (I)    double containing row at which
 *              line will start (0 is topmost)
 *      col0 (I)    double containing col at which
 *              line will start (0 is leftmost)
 *      row1 (I)    double containing row at which
 *              line will end (rp->height-1 is bottom-most)
 *      col1 (I)    double containing col at which
 *              line will end (rp->width-1 is rightmost)
 *      thickness (I)   int containing number of pixels/bits
 *              thick the line will be
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if line drawn okay,
 *              or 0 for any error.
 * --------------------------------------------------------------------------
 * Notes:     o Recurses, drawing left- and right-halves of line
 *      until a horizontal or vertical segment is found
 * ======================================================================= */
int line_recurse(raster * rp, double row0, double col0, double row1,
                 double col1, int thickness)
{
    double delrow = fabs(row1 - row0);  /* 0 if line horizontal */
    double delcol = fabs(col1 - col0);  /* 0 if line vertical */
    double tolerance = 0.5;     /* draw line when it goes to point */
    double midrow = 0.5 * (row0 + row1);        /* midpoint row */
    double midcol = 0.5 * (col0 + col1);        /* midpoint col */
/* -------------------------------------------------------------------------
recurse if either delta > tolerance
-------------------------------------------------------------------------- */
    if (delrow > tolerance      /* row hasn't converged */
        || delcol > tolerance) {        /* col hasn't converged */
        line_recurse(rp, row0, col0, midrow, midcol, thickness);        /* left half */
        line_recurse(rp, midrow, midcol, row1, col1, thickness);        /* right half */
        return 1;
    }
/* -------------------------------------------------------------------------
draw converged point
-------------------------------------------------------------------------- */
    setpixel(rp, iround(midrow), iround(midcol), 255);  /*set pixel at midrow,midcol */
    return 1;
}

/* ==========================================================================
 * Function:    circle_raster ( rp,  row0, col0,  row1, col1,
 *      thickness, quads )
 * Purpose: Draw quad(rant)s of an ellipse in box determined by
 *      diagonally opposite corner points (row0,col0) and
 *      (row1,col1), of thickness pixels in existing raster rp.
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster in which an ellipse
 *              will be drawn
 *      row0 (I)    int containing 1st corner row bounding ellipse
 *              (0 is topmost)
 *      col0 (I)    int containing 1st corner col bounding ellipse
 *              (0 is leftmost)
 *      row1 (I)    int containing 2nd corner row bounding ellipse
 *              (rp->height-1 is bottom-most)
 *      col1 (I)    int containing 2nd corner col bounding ellipse
 *              (rp->width-1 is rightmost)
 *      thickness (I)   int containing number of pixels/bits
 *              thick the ellipse arc line will be
 *      quads (I)   char * to null-terminated string containing
 *              any subset/combination of "1234" specifying
 *              which quadrant(s) of ellipse to draw.
 *              NULL ptr draws all four quadrants;
 *              otherwise 1=upper-right quadrant,
 *              2=uper-left, 3=lower-left, 4=lower-right,
 *              i.e., counterclockwise from 1=positive quad.
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if ellipse drawn okay,
 *              or 0 for any error.
 * --------------------------------------------------------------------------
 * Notes:     o row0==row1 or col0==col1 are errors
 *        o using ellipse equation x^2/a^2 + y^2/b^2 = 1
 * ======================================================================= */
static int circle_raster(raster * rp, int row0, int col0, int row1,
                         int col1, int thickness, const char *quads)
{
/* --- lower-left and upper-right bounding points (in our coords) --- */
    int lorow = min2(row0, row1),       /* lower bounding row (top of box) */
        locol = min2(col0, col1),       /* lower bounding col (left of box) */
        hirow = max2(row0, row1),       /* upper bounding row (bot of box) */
        hicol = max2(col0, col1);       /* upper bounding col (right of box) */
/* --- a and b ellipse params --- */
    int width = hicol - locol + 1,      /* width of bounding box */
        height = hirow - lorow + 1,     /* height of bounding box */
        islandscape = (width >= height ? 1 : 0);        /*true if ellipse lying on side */
    double a = ((double) width) / 2.0,  /* x=a when y=0 */
        b = ((double) height) / 2.0,    /* y=b when x=0 */
        abmajor = (islandscape ? a : b),        /* max2(a,b) */
        abminor = (islandscape ? b : a),        /* min2(a,b) */
        abmajor2 = abmajor * abmajor,   /* abmajor^2 */
        abminor2 = abminor * abminor;   /* abminor^2 */
/* --- other stuff --- */
    int imajor = 0, nmajor = max2(width, height),       /*index, #pixels on major axis */
        iminor = 0, nminor = min2(width, height);       /* solved index on minor axis */
    int irow, icol,             /* raster indexes at circumference */
     rsign = 1, csign = 1;      /* row,col signs, both +1 in quad 1 */
    double midrow = ((double) (row0 + row1)) / 2.0,     /* center row */
        midcol = ((double) (col0 + col1)) / 2.0;        /* center col */
    double xy, xy2,             /* major axis ellipse coord */
     yx2, yx;                   /* solved minor ellipse coord */
    int isokay = 1;             /* true if no pixels out-of-bounds */
    const char *qptr = NULL;
    const char *allquads = "1234";      /* quadrants if input quads==NULL */
    int isrecurse = 1;          /* true to draw ellipse recursively */
/* -------------------------------------------------------------------------
pixel-by-pixel along positive major axis, quit when it goes negative
-------------------------------------------------------------------------- */
    if (quads == NULL)
        quads = allquads;       /* draw all quads, or only user's */
    if (msgfp != NULL && msglevel >= 39)        /* debugging */
        fprintf(msgfp, "circle_raster> width,height;quads=%d,%d,%s\n",
                width, height, quads);
    if (nmajor < 1) {
        isokay = 0;             /* problem with input args */
    } else {
        if (isrecurse) {        /* use recursive algorithm */
            for (qptr = quads; *qptr != '\0'; qptr++) { /* for each character in quads */
                double theta0 = 0.0, theta1 = 0.0;      /* set thetas based on quadrant */
                switch (*qptr) {        /* check for quadrant 1,2,3,4 */
                default:       /* unrecognized, assume quadrant 1 */
                case '1':
                    theta0 = 0.0;
                    theta1 = 90.0;
                    break;      /* first quadrant */
                case '2':
                    theta0 = 90.0;
                    theta1 = 180.0;
                    break;      /* second quadrant */
                case '3':
                    theta0 = 180.0;
                    theta1 = 270.0;
                    break;      /* third quadrant */
                case '4':
                    theta0 = 270.0;
                    theta1 = 360.0;
                    break;
                }               /* fourth quadrant */
                circle_recurse(rp, row0, col0, row1, col1, thickness,
                               theta0, theta1);
            }
            return 1;
        }
        for (imajor = (nmajor + 1) / 2;; imajor--) {
            /* --- xy is coord along major axis, yx is "solved" along minor axis --- */
            xy = ((double) imajor);     /* xy = abmajor ... 0 */
            if (xy < 0.0)
                break;          /* negative side symmetrical */
            yx2 = abminor2 * (1.0 - xy * xy / abmajor2);        /* "solve" ellipse equation */
            yx = (yx2 > 0.0 ? sqrt(yx2) : 0.0); /* take sqrt if possible */
            iminor = iround(yx);        /* nearest integer */
            /* --- set pixels for each requested quadrant --- */
            for (qptr = quads; *qptr != '\0'; qptr++) { /* for each character in quads */
                rsign = (-1);
                csign = 1;      /* init row,col in user quadrant 1 */
                switch (*qptr) {        /* check for quadrant 1,2,3,4 */
                default:
                    break;      /* unrecognized, assume quadrant 1 */
                case '4':
                    rsign = 1;
                    break;      /* row,col both pos in quadrant 4 */
                case '3':
                    rsign = 1;  /* row pos, col neg in quadrant 3 */
                case '2':
                    csign = (-1);
                    break;
                }               /* row,col both neg in quadrant 2 */
                irow =
                    iround(midrow +
                           (double) rsign * (islandscape ? yx : xy));
                irow = min2(hirow, max2(lorow, irow));  /* keep irow in bounds */
                icol =
                    iround(midcol +
                           (double) csign * (islandscape ? xy : yx));
                icol = min2(hicol, max2(locol, icol));  /* keep icol in bounds */
                if (msgfp != NULL && msglevel >= 49)    /* debugging */
                    fprintf(msgfp,
                            "\t...imajor=%d; iminor,quad,irow,icol=%d,%c,%d,%d\n",
                            imajor, iminor, *qptr, irow, icol);
                if (irow < 0 || irow >= rp->height      /* row outside raster */
                    || icol < 0 || icol >= rp->width) { /* col outside raster */
                    isokay = 0; /* signal out-of-bounds pixel */
                    continue;
                }               /* but still try remaining points */
                setpixel(rp, irow, icol, 255);  /* set pixel at irow,icol */
            }
        }
        /* ------------------------------------------------------------------------
           now do it _again_ along minor axis to avoid "gaps"
           ------------------------------------------------------------------------- */
        if (1 && iminor > 0) {
            for (iminor = (nminor + 1) / 2;; iminor--) {
                /* --- yx is coord along minor axis, xy is "solved" along major axis --- */
                yx = ((double) iminor); /* yx = abminor ... 0 */
                if (yx < 0.0) {
                    break;
                }
                xy2 = abmajor2 * (1.0 - yx * yx / abminor2);    /* "solve" ellipse equation */
                xy = (xy2 > 0.0 ? sqrt(xy2) : 0.0);     /* take sqrt if possible */
                imajor = iround(xy);    /* nearest integer */
                /* --- set pixels for each requested quadrant --- */
                for (qptr = quads; *qptr != '\0'; qptr++) {     /* for each character in quads */
                    rsign = -1;
                    csign = 1;  /* init row,col in user quadrant 1 */
                    switch (*qptr) {    /* check for quadrant 1,2,3,4 */
                    default:
                        break;  /* unrecognized, assume quadrant 1 */
                    case '4':
                        rsign = 1;
                        break;  /* row,col both pos in quadrant 4 */
                    case '3':
                        rsign = 1;      /* row pos, col neg in quadrant 3 */
                        // XXX:fall through???
                    case '2':
                        csign = -1;
                        break;
                    }           /* row,col both neg in quadrant 2 */
                    irow = iround(midrow + (double) rsign * (islandscape ? yx : xy));
                    irow = min2(hirow, max2(lorow, irow));      /* keep irow in bounds */
                    icol = iround(midcol + (double) csign * (islandscape ? xy : yx));
                    icol = min2(hicol, max2(locol, icol));      /* keep icol in bounds */
                    if (msgfp != NULL && msglevel >= 49)        /* debugging */
                        fprintf(msgfp,
                                "\t...iminor=%d; imajor,quad,irow,icol=%d,%c,%d,%d\n",
                                iminor, imajor, *qptr, irow, icol);
                    if (irow < 0 || irow >= rp->height || icol < 0 || icol >= rp->width) {
                        isokay = 0;     /* signal out-of-bounds pixel */
                        continue;
                    }
                    setpixel(rp, irow, icol, 255);      /* set pixel at irow,icol */
                }
            }
        }
    }
    return isokay;
}

/* ==========================================================================
 * Function:    circle_recurse ( rp,  row0, col0,  row1, col1,
 *      thickness, theta0, theta1 )
 * Purpose: Recursively draws arc theta0<=theta<=theta1 of the ellipse
 *      in box determined by diagonally opposite corner points
 *      (row0,col0) and (row1,col1), of thickness pixels in raster rp.
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster in which an ellipse
 *              will be drawn
 *      row0 (I)    int containing 1st corner row bounding ellipse
 *              (0 is topmost)
 *      col0 (I)    int containing 1st corner col bounding ellipse
 *              (0 is leftmost)
 *      row1 (I)    int containing 2nd corner row bounding ellipse
 *              (rp->height-1 is bottom-most)
 *      col1 (I)    int containing 2nd corner col bounding ellipse
 *              (rp->width-1 is rightmost)
 *      thickness (I)   int containing number of pixels/bits
 *              thick the ellipse arc line will be
 *      theta0 (I)  double containing first angle -360 -> +360
 *      theta1 (I)  double containing second angle -360 -> +360
 *              0=x-axis, positive moving counterclockwise
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if ellipse drawn okay,
 *              or 0 for any error.
 * --------------------------------------------------------------------------
 * Notes:     o row0==row1 or col0==col1 are errors
 *        o using ellipse equation x^2/a^2 + y^2/b^2 = 1
 *      Then, with x=r*cos(theta), y=r*sin(theta), ellipse
 *      equation is r = ab/sqrt(a^2*sin^2(theta)+b^2*cos^2(theta))
 * ======================================================================= */
static int circle_recurse(raster * rp, int row0, int col0,
                          int row1, int col1, int thickness, double theta0,
                          double theta1)
{
/* --- lower-left and upper-right bounding points (in our coords) --- */
    int lorow = min2(row0, row1);       /* lower bounding row (top of box) */
    int locol = min2(col0, col1);       /* lower bounding col (left of box) */
    int hirow = max2(row0, row1);       /* upper bounding row (bot of box) */
    int hicol = max2(col0, col1);       /* upper bounding col (right of box) */
/* --- a and b ellipse params --- */
    int width = hicol - locol + 1;      /* width of bounding box */
    int height = hirow - lorow + 1;     /* height of bounding box */
    double a = ((double) width) / 2.0;  /* col x=a when row y=0 */
    double b = ((double) height) / 2.0;    /* row y=b when col x=0 */
    double ab = a * b, a2 = a * a, b2 = b * b;     /* product and squares */
/* --- arc parameters --- */
    double rads = 0.017453292,  /* radians per degree = 1/57.29578 */
        lotheta = rads * dmod(min2(theta0, theta1), 360),       /* smaller angle */
        hitheta = rads * dmod(max2(theta0, theta1), 360),       /* larger angle */
        locos = cos(lotheta), losin = sin(lotheta),     /* trigs for lotheta */
        hicos = cos(hitheta), hisin = sin(hitheta),     /* trigs for hitheta */
        rlo = ab / sqrt(b2 * locos * locos + a2 * losin * losin),       /* r for lotheta */
        rhi = ab / sqrt(b2 * hicos * hicos + a2 * hisin * hisin),       /* r for hitheta */
        xlo = rlo * locos, ylo = rlo * losin,   /*col,row pixel coords for lotheta */
        xhi = rhi * hicos, yhi = rhi * hisin,   /*col,row pixel coords for hitheta */
        xdelta = fabs(xhi - xlo), ydelta = fabs(yhi - ylo),     /* col,row deltas */
        tolerance = 0.5;        /* convergence tolerance */
/* -------------------------------------------------------------------------
recurse if either delta > tolerance
-------------------------------------------------------------------------- */
    if (ydelta > tolerance || xdelta > tolerance) {        /* row, col hasn't converged */
        double midtheta = 0.5 * (theta0 + theta1);      /* mid angle for arc */
        circle_recurse(rp, row0, col0, row1, col1, thickness, theta0, midtheta);        /*lo */
        circle_recurse(rp, row0, col0, row1, col1, thickness, midtheta, theta1);
    } else {
    /* -------------------------------------------------------------------------
       draw converged point
       -------------------------------------------------------------------------- */
        double xcol = 0.5 * (xlo + xhi), yrow = 0.5 * (ylo + yhi);      /* relative to center */
        double centerrow = 0.5 * ((double) (lorow + hirow));       /* ellipse y-center */
        double centercol = 0.5 * ((double) (locol + hicol));       /* ellipse x-center */
        double midrow = centerrow - yrow, midcol = centercol + xcol;       /* pixel coords */
        setpixel(rp, iround(midrow), iround(midcol), 255);
    }                           /* set midrow,midcol */
    return 1;
}

/* ==========================================================================
 * Function:    bezier_raster ( rp, r0,c0, r1,c1, rt,ct )
 * Purpose: Recursively draw bezier from r0,c0 to r1,c1
 *      (with tangent point rt,ct) in existing raster rp.
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster in which a line
 *              will be drawn
 *      r0 (I)      double containing row at which
 *              bezier will start (0 is topmost)
 *      c0 (I)      double containing col at which
 *              bezier will start (0 is leftmost)
 *      r1 (I)      double containing row at which
 *              bezier will end (rp->height-1 is bottom-most)
 *      c1 (I)      double containing col at which
 *              bezier will end (rp->width-1 is rightmost)
 *      rt (I)      double containing row for tangent point
 *      ct (I)      double containing col for tangent point
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if line drawn okay,
 *              or 0 for any error.
 * --------------------------------------------------------------------------
 * Notes:     o Recurses, drawing left- and right-halves of bezier curve
 *      until a point is found
 * ======================================================================= */
static int bezier_raster(raster * rp, double r0, double c0, double r1,
                         double c1, double rt, double ct)
{
    double delrow = fabs(r1 - r0),      /* 0 if same row */
        delcol = fabs(c1 - c0), /* 0 if same col */
        tolerance = 0.5;        /* draw curve when it goes to point */
    double midrow = 0.5 * (r0 + r1),    /* midpoint row */
        midcol = 0.5 * (c0 + c1);       /* midpoint col */
    int irow = 0, icol = 0;     /* point to be drawn */
    int status = 1;             /* return status */
/* -------------------------------------------------------------------------
recurse if either delta > tolerance
-------------------------------------------------------------------------- */
    if (delrow > tolerance      /* row hasn't converged */
        || delcol > tolerance) {        /* col hasn't converged */
        bezier_raster(rp, r0, c0,       /* left half */
                      0.5 * (rt + midrow), 0.5 * (ct + midcol),
                      0.5 * (r0 + rt), 0.5 * (c0 + ct));
        bezier_raster(rp, 0.5 * (rt + midrow), 0.5 * (ct + midcol),     /* right half */
                      r1, c1, 0.5 * (r1 + rt), 0.5 * (c1 + ct));
        return 1;
    }
/* -------------------------------------------------------------------------
draw converged point
-------------------------------------------------------------------------- */
/* --- get integer point --- */
    irow = iround(midrow);      /* row pixel coord */
    icol = iround(midcol);      /* col pixel coord */
/* --- bounds check --- */
    if (irow >= 0 && irow < rp->height  /* row in bounds */
        && icol >= 0 && icol < rp->width)       /* col in bounds */
        setpixel(rp, irow, icol, 255);  /* so set pixel at irow,icol */
    else
        status = 0;             /* bad status if out-of-bounds */
    return status;
}

/* ==========================================================================
 * Function:    border_raster ( rp, ntop, nbot, isline, isfree )
 * Purpose: Allocate a new raster containing a copy of input rp,
 *      along with ntop extra rows at top and nbot at bottom,
 *      and whose width is either adjusted correspondingly,
 *      or is automatically enlarged to a multiple of 8
 *      with original bitmap centered
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster on which a border
 *              is to be placed
 *      ntop (I)    int containing number extra rows at top.
 *              if negative, abs(ntop) used, and same
 *              number of extra cols added at left.
 *      nbot (I)    int containing number extra rows at bottom.
 *              if negative, abs(nbot) used, and same
 *              number of extra cols added at right.
 *      isline (I)  int containing 0 to leave border pixels clear
 *              or >0 to draw a line around border of
 *              thickness isline pixels.  See Notes below.
 *      isfree (I)  int containing true to free rp before return
 * --------------------------------------------------------------------------
 * Returns: ( raster * )    ptr to bordered raster,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o The isline arg also controls which sides border lines
 *      are drawn for.  To do this, isline is interpreted as
 *      thickness + 100*sides  so that, e.g., passing isline=601
 *      is interpreted as sides=6 and thickness=1.  And
 *      sides is further interpreted as 1=left side, 2=top,
 *      4=right and 8=bottom.  For example, sides=6 where 6=2+4
 *      draws the top and right borders only.  15 draws all four
 *      sides.  And 0 (no sides value embedded in isline)
 *      draws all four sides, too.
 * ======================================================================= */
static raster *border_raster(raster * rp, int ntop, int nbot, int isline,
                             int isfree)
{
    raster *bp = NULL;          /*raster back to caller */
    int width = (rp == NULL ? 0 : rp->width),   /* width of raster */
        height = (rp == NULL ? 0 : rp->height), /* height of raster */
        istopneg = 0, isbotneg = 0,     /* true if ntop or nbot negative */
        leftmargin = 0;         /* adjust width to whole number of bytes */
    int left = 1, top = 1, right = 1, bot = 1;  /* frame sides to draw */
/* -------------------------------------------------------------------------
Initialization
-------------------------------------------------------------------------- */
    if (rp == NULL)
        goto end_of_job;        /* no input raster provided */
    if (isstring || (1 && rp->height == 1)) {   /* explicit string signal or infer */
        bp = rp;
        goto end_of_job;
    }
    /* return ascii string unchanged */
    /* --- check for negative args --- */
    if (ntop < 0) {
        ntop = -ntop;
        istopneg = 1;
    }                           /*flip positive and set flag */
    if (nbot < 0) {
        nbot = -nbot;
        isbotneg = 1;
    }
    /*flip positive and set flag */
    /* --- adjust height for ntop and nbot margins --- */
    height += (ntop + nbot);    /* adjust height for margins */
/* --- adjust width for left and right margins --- */
    if (istopneg || isbotneg) { /*caller wants nleft=ntop and/or nright=nbot *//* --- adjust width (and leftmargin) as requested by caller -- */
        if (istopneg) {
            width += ntop;
            leftmargin = ntop;
        }
        if (isbotneg)
            width += nbot;
    } else {                    /* --- or adjust width (and leftmargin) to whole number of bytes --- */
        leftmargin = (width % 8 == 0 ? 0 : 8 - (width % 8));    /*makes width multiple of 8 */
        width += leftmargin;    /* width now multiple of 8 */
        leftmargin /= 2;
    }                           /* center original raster */
/* --- check which sides to draw --- */
    if (isline > 100) {         /* sides arg embedded in isline */
        int iside = 0, sides = isline / 100;    /* index, sides=1-15 from 101-1599 */
        isline -= 100 * sides;  /* and remove sides from isline */
        for (iside = 1; iside <= 4; iside++) {  /* check left, top, right, bot */
            int shift = sides / 2;      /* shift sides left one bit */
            if (sides == 2 * shift)     /* low-order bit is >>not<< set */
                switch (iside) {        /* don't draw corresponding side */
                default:
                    break;      /* internal error */
                case 1:
                    left = 0;
                    break;      /* 1 = left side */
                case 2:
                    top = 0;
                    break;      /* 2 = top side */
                case 3:
                    right = 0;
                    break;      /* 4 = tight side */
                case 4:
                    bot = 0;
                    break;
                }               /* 8 = bottom side */
            sides = shift;      /* ready for next side */
        }
    }

    /* -------------------------------------------------------------------------
       allocate bordered raster, and embed rp within it
       -------------------------------------------------------------------------- */
    /* --- allocate bordered raster --- */
    if ((bp = new_raster(width, height, rp->pixsz)) == NULL) {
        goto end_of_job;        /* and quit if failed */
    }
/* --- embed rp in it --- */
    rastput(bp, rp, ntop, leftmargin, 1);       /* rp embedded in bp */
/* -------------------------------------------------------------------------
draw border if requested
-------------------------------------------------------------------------- */
    if (isline) {
        int irow, icol, nthick = isline;        /*height,width index, line thickness */
        /* --- draw left- and right-borders --- */
        for (irow = 0; irow < height; irow++) {   /* for each row of bp */
            for (icol = 0; icol < nthick; icol++) {     /* and each pixel of thickness */
                if (left) {
                    setpixel(bp, irow, icol, 255);
                }               /* left border */
                if (right) {
                    setpixel(bp, irow, width - 1 - icol, 255);
                }
            }                   /* right border */
        }
        /* --- draw top- and bottom-borders --- */
        for (icol = 0; icol < width; icol++) {   /* for each col of bp */
            for (irow = 0; irow < nthick; irow++) {     /* and each pixel of thickness */
                if (top) {
                    setpixel(bp, irow, icol, 255);
                }               /* top border */
                if (bot) {
                    setpixel(bp, height - 1 - irow, icol, 255);
                }
            }                   /* bottom border */
        }
    }

    /* -------------------------------------------------------------------------
       free rp if no longer needed
       -------------------------------------------------------------------------- */
    if (isfree) {               /*caller no longer needs rp */
        delete_raster(rp);      /* so free it for him */
    }

  end_of_job:
    return bp;
}

/* ==========================================================================
 * Function:    backspace_raster ( rp, nback, pback, minspace, isfree )
 * Purpose: Allocate a new raster containing a copy of input rp,
 *      but with trailing nback columns removed.
 *      If minspace>=0 then (at least) that many columns
 *      of whitespace will be left in place, regardless of nback.
 *      If minspace<0 then existing black pixels will be deleted
 *      as required.
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster on which a border
 *              is to be placed
 *      nback (I)   int containing number of columns to
 *              backspace (>=0)
 *      pback (O)   ptr to int returning #pixels actually
 *              backspaced (or NULL to not use)
 *      minspace (I)    int containing number of columns
 *              of whitespace to be left in place
 *      isfree (I)  int containing true to free rp before return
 * --------------------------------------------------------------------------
 * Returns: ( raster * )    ptr to backspaced raster,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o For \! negative space, for \hspace{-10}, etc.
 * ======================================================================= */
static raster *backspace_raster(raster * rp, int nback, int *pback,
                                int minspace, int isfree)
{
    raster *bp = NULL;          /* raster returned to caller */
    int width = (rp == NULL ? 0 : rp->width);   /* original width of raster */
    int height = (rp == NULL ? 0 : rp->height); /* height of raster */
    int mback = nback;          /* nback adjusted for minspace */
    int newwidth = width;       /* adjusted width after backspace */
    int icol = 0, irow = 0;     /* col,row index */

    if (rp == NULL) {
        goto end_of_job;        /* no input given */
    }
/* -------------------------------------------------------------------------
locate rightmost column of rp containing ink, and determine backspaced width
-------------------------------------------------------------------------- */
/* --- locate rightmost column of rp containing ink --- */
    if (minspace >= 0) {         /* only needed if given minspace */
        for (icol = width - 1; icol >= 0; icol--) {      /* find first non-empty col in row */
            for (irow = 0; irow < height; irow++) {      /* for each row inside rp */
                if (getpixel(rp, irow, icol) != 0) {    /* found a set pixel */
                    int whitecols = (width - 1) - icol; /* #cols containing white space */
                    mback = min2(nback, max2(0, whitecols - minspace)); /*leave minspace cols */
                    goto gotright;
                }
            }
        }
    }
    /* no need to look further */
    /* --- determine width of new backspaced raster --- */
  gotright:                    /* found col with ink (or rp empty) */
    if (mback > width) {
        mback = width;          /* can't backspace before beginning */
    }
    newwidth = max2(1, width - mback);  /* #cols in backspaced raster */
    if (pback != NULL) {
        *pback = width - newwidth;      /* caller wants #pixels */
    }
/* -------------------------------------------------------------------------
allocate new raster and fill it with leftmost cols of rp
-------------------------------------------------------------------------- */
/* --- allocate backspaced raster --- */
    if ((bp = new_raster(newwidth, height, rp->pixsz)) == NULL) {
        goto end_of_job;
    }
/* --- fill new raster --- */
    if (1 || width - nback > 0) { /* don't fill 1-pixel wide empty bp */
        for (icol = 0; icol < newwidth; icol++) { /* find first non-empty col in row */
            for (irow = 0; irow < height; irow++) {     /* for each row inside rp */
                int value = getpixel(rp, irow, icol);   /* original pixel at irow,icol */
                setpixel(bp, irow, icol, value);
            }
        }
    }

    /* saved in backspaced raster */
    /* -------------------------------------------------------------------------
       Back to caller with backspaced raster (or null for any error)
       -------------------------------------------------------------------------- */
  end_of_job:
    if (msgfp != NULL && msglevel >= 999) {
        fprintf(msgfp,          /* diagnostics */
                "backspace_raster> nback=%d,minspace=%d,mback=%d, width:old=%d,new=%d\n",
                nback, minspace, mback, width, newwidth);
        fflush(msgfp);
    }
    if (isfree && bp != NULL) {
        delete_raster(rp);      /* free original raster */
    }
    return bp;
}

/* ==========================================================================
 * Function:    type_raster ( rp, fp )
 * Purpose: Emit an ascii dump representing rp, on fp.
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      ptr to raster struct for which an
 *              ascii dump is to be constructed.
 *      fp (I)      File ptr to output device (defaults to
 *              stdout if passed as NULL).
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if completed successfully,
 *              or 0 otherwise (for any error).
 * --------------------------------------------------------------------------
 * Notes:
 * ======================================================================= */
int type_raster(raster * rp, FILE *fp)
{
    static int display_width = 72;      /* max columns for display */
    static char display_chars[] = " 123456789BCDE*";    /* display chars for bytemap */
    char scanline[133];         /* ascii image for one scan line */
    int scan_width;             /* #chars in scan (<=display_width) */
    int irow, locol, hicol = (-1);      /* height index, width indexes */
    raster *bitmaprp = rp;      /* convert .gf to bitmap if needed */
/* --------------------------------------------------------------------------
initialization
-------------------------------------------------------------------------- */
/* --- redirect null fp --- */
    if (fp == NULL) {
        fp = stdout;            /* default fp to stdout if null */
    }
    if (msglevel >= 999) {
        fprintf(fp,             /* debugging diagnostics */
                "type_raster> width=%d height=%d ...\n",
                rp->width, rp->height);
        fflush(fp);
    }
/* --- check for ascii string --- */
    if (isstring                /* pixmap has string, not raster */
        || (0 && rp->height == 1)) {    /* infer input rp is a string */
        char *string = (char *) (rp->pixmap);   /*interpret pixmap as ascii string */
        int width = strlen(string);     /* #chars in ascii string */
        while (width > display_width - 2) {     /* too big for one line */
            fprintf(fp, "\"%.*s\"\n", display_width - 2, string);       /*display leading chars */
            string += (display_width - 2);      /* bump string past displayed chars */
            width -= (display_width - 2);
        }                       /* decrement remaining width */
        fprintf(fp, "\"%.*s\"\n", width, string);       /* display trailing chars */
        return 1;
    }
    /* --------------------------------------------------------------------------
       display ascii dump of bitmap image (in segments if display_width < rp->width)
       -------------------------------------------------------------------------- */
    if (rp->format == 2         /* input is .gf-formatted */
        || rp->format == 3)
        bitmaprp = gftobitmap(rp);      /* so convert it for display */
    if (bitmaprp != NULL) {      /* if we have image for display */
        while ((locol = hicol + 1) < rp->width) {       /*start where prev segment left off */
            /* --- set hicol for this pass (locol set above) --- */
            hicol += display_width;     /* show as much as display allows */
            if (hicol >= rp->width)
                hicol = rp->width - 1;  /*but not more than raster */
            scan_width = hicol - locol + 1;     /* #chars in this scan */
            if (locol > 0)
                fprintf(fp, "----------\n");    /*separator between segments */
            /* ------------------------------------------------------------------------
               display all scan lines for this local...hicol segment range
               ------------------------------------------------------------------------ */
            for (irow = 0; irow < rp->height; irow++) { /* all scan lines for col range */
                /* --- allocations and declarations --- */
                int ipix,       /* pixmap[] index for this scan */
                 lopix = irow * rp->width + locol;      /*first pixmap[] pixel in this scan */
                /* --- set chars in scanline[] based on pixels in rp->pixmap[] --- */
                for (ipix = 0; ipix < scan_width; ipix++) {       /* set each char */
                    if (bitmaprp->pixsz == 1) {   /*' '=0 or '*'=1 to display bitmap */
                        scanline[ipix] = (getlongbit(bitmaprp->pixmap, lopix + ipix) == 1 ? '*' : '.');
                    } else if (bitmaprp->pixsz == 8) {        /* double-check pixsz for bytemap */
                        int pixval = (int) (bitmaprp->pixmap[lopix + ipix]);  /*byte value */
                        int ichar = min2(15, pixval / 16);      /* index for ' ', '1'...'e', '*' */
                        scanline[ipix] = display_chars[ichar];
                    }
                }
                /*set ' ' for 0-15, etc */
                /* --- display completed scan line --- */
                fprintf(fp, "%.*s\n", scan_width, scanline);
            }
        }
    }

    /* -------------------------------------------------------------------------
       Back to caller with 1=okay, 0=failed.
       -------------------------------------------------------------------------- */
    if (rp->format == 2         /* input was .gf-format */
        || rp->format == 3)
        if (bitmaprp != NULL)   /* and we converted it for display */
            delete_raster(bitmaprp);    /* no longer needed, so free it */
    return 1;
}

/* ==========================================================================
 * Function:    type_bytemap ( bp, grayscale, width, height, fp )
 * Purpose: Emit an ascii dump representing bp, on fp.
 * --------------------------------------------------------------------------
 * Arguments:   bp (I)      intbyte * to bytemap for which an
 *              ascii dump is to be constructed.
 *      grayscale (I)   int containing #gray shades, 256 for 8-bit
 *      width (I)   int containing #cols in bytemap
 *      height (I)  int containing #rows in bytemap
 *      fp (I)      File ptr to output device (defaults to
 *              stdout if passed as NULL).
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if completed successfully,
 *              or 0 otherwise (for any error).
 * --------------------------------------------------------------------------
 * Notes:
 * ======================================================================= */
/* --- entry point --- */
int type_bytemap(intbyte * bp, int grayscale,
                 int width, int height, FILE * fp)
{
/* -------------------------------------------------------------------------
Allocations and Declarations
-------------------------------------------------------------------------- */
    static int display_width = 72;      /* max columns for display */
    int byte_width = 3,         /* cols to display byte (ff+space) */
        maxbyte = 0;            /* if maxbyte<16, set byte_width=2 */
    int white_byte = 0,         /* show dots for white_byte's */
        black_byte = grayscale - 1;     /* show stars for black_byte's */
    char scanline[133];         /* ascii image for one scan line */
    int scan_width,             /* #chars in scan (<=display_width) */
     scan_cols;                 /* #cols in scan (hicol-locol+1) */
    int ibyte,                  /* bp[] index */
     irow, locol, hicol = (-1); /* height index, width indexes */
/* --------------------------------------------------------------------------
initialization
-------------------------------------------------------------------------- */
/* --- redirect null fp --- */
    if (fp == (FILE *) NULL)
        fp = stdout;            /* default fp to stdout if null */
/* --- check for ascii string --- */
    if (isstring) {             /* bp has ascii string, not raster */
        width = strlen((char *) bp);    /* #chars in ascii string */
        height = 1;
    }
    /* default */
    /* --- see if we can get away with byte_width=1 --- */
    for (ibyte = 0; ibyte < width * height; ibyte++) {  /* check all bytes */
        int byteval = (int) bp[ibyte];  /* current byte value */
        if (byteval < black_byte)       /* if it's less than black_byte */
            maxbyte = max2(maxbyte, byteval);
    }                           /* then find max non-black value */
    if (maxbyte < 16)           /* bytevals will fit in one column */
        byte_width = 1;         /* so reset display byte_width */
/* --------------------------------------------------------------------------
display ascii dump of bitmap image (in segments if display_width < rp->width)
-------------------------------------------------------------------------- */
    while ((locol = hicol + 1) < width) {       /*start where prev segment left off */
        /* --- set hicol for this pass (locol set above) --- */
        hicol += display_width / byte_width;    /* show as much as display allows */
        if (hicol >= width)
            hicol = width - 1;  /* but not more than bytemap */
        scan_cols = hicol - locol + 1;  /* #cols in this scan */
        scan_width = byte_width * scan_cols;    /* #chars in this scan */
        if (locol > 0 && !isstring)
            fprintf(fp, "----------\n");        /* separator */
        /* ------------------------------------------------------------------------
           display all scan lines for this local...hicol segment range
           ------------------------------------------------------------------------ */
        for (irow = 0; irow < height; irow++) { /* all scan lines for col range */
            /* --- allocations and declarations --- */
            int lobyte = irow * width + locol;  /* first bp[] byte in this scan */
            char scanbyte[32];  /* sprintf() buffer for byte */
            /* --- set chars in scanline[] based on bytes in bytemap bp[] --- */
            memset(scanline, ' ', scan_width);  /* blank out scanline */
            for (ibyte = 0; ibyte < scan_cols; ibyte++) {       /* set chars for each col */
                int byteval = (int) bp[lobyte + ibyte]; /* value of current byte */
                memset(scanbyte, '.', byte_width);      /* dot-fill scanbyte */
                if (byteval == black_byte)      /* but if we have a black byte */
                    memset(scanbyte, '*', byte_width);  /* star-fill scanbyte instead */
                if (byte_width > 1)     /* don't blank out single char */
                    scanbyte[byte_width - 1] = ' ';     /* blank-fill rightmost character */
                if (byteval != white_byte       /* format bytes that are non-white */
                    && byteval != black_byte)   /* and that are non-black */
                    sprintf(scanbyte, "%*x ", max2(1, byte_width - 1), byteval);        /*hex-format */
                memcpy(scanline + ibyte * byte_width, scanbyte,
                       byte_width);
            }                   /*in line */
            /* --- display completed scan line --- */
            fprintf(fp, "%.*s\n", scan_width, scanline);
        }
    }
    return 1;
}

/* ==========================================================================
 * Function:    gftobitmap ( gf )
 * Purpose: convert .gf-like pixmap to bitmap image
 * --------------------------------------------------------------------------
 * Arguments:   gf (I)      raster * to struct in .gf-format
 * --------------------------------------------------------------------------
 * Returns: ( raster * )    image-format raster * if successful,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static raster *gftobitmap(raster * gf)
{
    raster *rp = NULL;          /* image raster retuned to caller */
    int width = 0, height = 0, totbits = 0;     /* gf->width, gf->height, #bits */
    int format = 0, icount = 0, ncounts = 0;    /*.gf format, count index, #counts */
    int ibit = 0, bitval = 0;   /* bitmap index, bit value */
    int isrepeat = 1;           /* true to process repeat counts */
    int repeatcmds[2] = { 255, 15 };        /*opcode for repeat/duplicate count */
    int nrepeats = 0, irepeat = 0;      /* scan line repeat count,index */
    int wbits = 0;              /* count bits to width of scan line */

    if (gf == NULL) {
        goto end_of_job;        /* input raster not provided */
    }
    format = gf->format;        /* 2 or 3 */
    if (format != 2 && format != 3) {
        goto end_of_job;        /* invalid raster format */
    }
    ncounts = gf->pixsz;        /*pixsz is really #counts in pixmap */

/* --- allocate output raster with proper dimensions for bitmap --- */
    width = gf->width;
    height = gf->height;        /* dimensions of raster */
    if ((rp = new_raster(width, height, 1)) == NULL) {
        goto end_of_job;
    }
    totbits = width * height;   /* total #bits in image */
/* -------------------------------------------------------------------------
fill bitmap
-------------------------------------------------------------------------- */
    for (icount = 0, bitval = 0; icount < ncounts; icount++) {
        int nbits = (int) (getbyfmt(format, gf->pixmap, icount));       /*#bits to set */
        if (isrepeat            /* we're proxessing repeat counts */
            && nbits == repeatcmds[format - 2]) {       /* and repeat opcode found */
            if (nrepeats == 0) {        /* recursive repeat is error */
                nrepeats = (int) (getbyfmt(format, gf->pixmap, icount + 1));    /*repeat count */
                nbits = (int) (getbyfmt(format, gf->pixmap, icount + 2));       /*#bits to set */
                icount += 2;
            } else if (msgfp != NULL && msglevel >= 1) {
                fprintf(msgfp,
                        "gftobitmap> found embedded repeat command\n");
            }
        }
        if (0) {
            fprintf(stdout,
                    "gftobitmap> icount=%d bitval=%d nbits=%d ibit=%d totbits=%d\n",
                    icount, bitval, nbits, ibit, totbits);
        }
        for (; nbits > 0; nbits--) {    /* count down */
            if (ibit >= totbits) {
                goto end_of_job;        /* overflow check */
            }
            for (irepeat = 0; irepeat <= nrepeats; irepeat++)
                if (bitval == 1) {      /* set pixel */
                    setlongbit(rp->pixmap, (ibit + irepeat * width));
                } else {        /* clear pixel */
                    unsetlongbit(rp->pixmap, (ibit + irepeat * width));
                }
            if (nrepeats > 0) {
                wbits++;        /* count another repeated bit */
            }
            ibit++;
        }                       /* bump bit index */
        bitval = 1 - bitval;    /* flip bit value */
        if (wbits >= width) {   /* completed repeats */
            ibit += nrepeats * width;   /*bump bit count past repeated scans */
            if (wbits > width) {  /* out-of alignment error */
                if (msgfp != NULL && msglevel >= 1) {    /* report error */
                    fprintf(msgfp, "gftobitmap> width=%d wbits=%d\n",
                            width, wbits);
                }
            }
            wbits = nrepeats = 0;
        }
    }
  end_of_job:
    return rp;
}


/* ==========================================================================
 * Function:    get_symdef ( symbol )
 * Purpose: returns mathchardef struct for symbol
 * --------------------------------------------------------------------------
 * Arguments:   symbol (I)  char *  containing symbol
 *              whose corresponding mathchardef is wanted
 * --------------------------------------------------------------------------
 * Returns: ( mathchardef * )  pointer to struct defining symbol,
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o Input symbol need only contain a leading substring to match,
 *      e.g., \gam passed in symbol will match \gamma in the table.
 *      If the table contains two or more possible matches,
 *      the shortest is returned, e.g., input \e will return with
 *      data for \eta rather than \epsilon.  To get \epsilon,
 *      you must pass a leading substring long enough to eliminate
 *      shorter table matches, i.e., in this case \ep
 * ======================================================================= */

static mathchardef *get_symdef(const char *symbol)
{
    mathchardef *symdefs = symtable;    /* table of mathchardefs */
    int ligdef = 0;             /* or we may have a ligature */
    int idef = 0;               /* symdefs[] index */
    int bestdef = -9999;        /*index of shortest matching symdef */
    int symlen = strlen(symbol);        /* length of input symbol */
    int deflen, minlen = 9999;  /*length of shortest matching symdef */

    /*alnumsym = (symlen==1 && isalnum(*symbol)), */
    /*alphanumeric sym */
    int alphasym = (symlen == 1 && isalpha(*symbol));
    /* or alpha symbol */
    int slashsym = (*symbol == '\\');   /* or \backslashed symbol */
    int family = fontinfo[fontnum].family;      /* current font family */
    static const char *displaysyms[][2] = {     /*xlate to Big sym for \displaystyle */
        /* --- see table on page 536 in TLC2 --- */
        {"\\int", "\\Bigint"},
        {"\\oint", "\\Bigoint"},
        {"\\sum", "\\Bigsum"},
        {"\\prod", "\\Bigprod"},
        {"\\coprod", "\\Bigcoprod"},
        /* --- must be 'big' when related to similar binary operators --- */
        {"\\bigcup", "\\Bigcup"},
        {"\\bigsqcup", "\\Bigsqcup"},
        {"\\bigcap", "\\Bigcap"},
        /*{"\\bigsqcap", "\\sqcap"}, *//* don't have \Bigsqcap */
        {"\\bigodot", "\\Bigodot"},
        {"\\bigoplus", "\\Bigoplus"},
        {"\\bigominus", "\\ominus"},
        {"\\bigotimes", "\\Bigotimes"},
        {"\\bigoslash", "\\oslash"},
        {"\\biguplus", "\\Biguplus"},
        {"\\bigwedge", "\\Bigwedge"},
        {"\\bigvee", "\\Bigvee"},
        {NULL, NULL}
    };
/* -------------------------------------------------------------------------
First check for ligature
-------------------------------------------------------------------------- */
    isligature = 0;             /* init signal for no ligature */
    if (family == CYR10)        /*only check for cyrillic ligatures */
        if ((ligdef = get_ligature(subexprptr, family)) >= 0) {
            /* found a ligature */
            bestdef = ligdef;   /* set bestdef for ligature */
            isligature = 1;     /* signal we found a ligature */
            goto end_of_job;
        }
    /* so just give it to caller */
    /* -------------------------------------------------------------------------
       If in \displaystyle mode, first xlate int to Bigint, etc.
       -------------------------------------------------------------------------- */
    if (isdisplaystyle > 1) {    /* we're in \displaystyle mode */
        for (idef = 0;; idef++) {       /* lookup symbol in displaysyms */
            const char *fromsym = displaysyms[idef][0]; /* look for this symbol */
            const char *tosym = displaysyms[idef][1];  /* and xlate it to this symbol */
            if (fromsym == NULL) {
                break;          /* end-of-table */
            }
            if (!strcmp(symbol, fromsym)) {     /* found a match */
                if (msglevel >= 99 && msgfp != NULL) {  /* debugging output */
                    fprintf(msgfp,
                            "get_symdef> isdisplaystyle=%d, xlated %s to %s\n",
                            isdisplaystyle, symbol, tosym);
                    fflush(msgfp);
                }
                symbol = tosym; /* so look up tosym instead */
                symlen = strlen(symbol);        /* reset symbol length */
                break;
            }                   /* no need to search further */
        }
    }
    /* -------------------------------------------------------------------------
       search symdefs[] in order for first occurrence of symbol
       -------------------------------------------------------------------------- */
    for (idef = 0;; idef++)     /* until trailer record found */
        if (symdefs[idef].symbol == NULL) {
            break;              /* reached end-of-table */
        } else if (strncmp(symbol, symdefs[idef].symbol, symlen) == 0) {       /* found match */
            if ((fontnum == 0 || family == CYR10)       /* mathmode, so check every match */
                ||(1 && symdefs[idef].handler != NULL)  /* or check every directive */
                ||(1 && istextmode && slashsym) /*text mode and \backslashed symbol */
                ||(0 && istextmode && (!alphasym        /* text mode and not alpha symbol */
                                       || symdefs[idef].handler != NULL))       /* or text mode and directive */
                ||(symdefs[idef].family == family       /* have correct family */
                   && symdefs[idef].handler == NULL))   /* and not a handler collision */
#if 0
                ||(fontnum == 1 && symdefs[idef].family == CMR10)       /*textmode && rm text */
                    ||(fontnum == 2 && symdefs[idef].family == CMMI10)  /*textmode && it text */
                    ||(fontnum == 3 && symdefs[idef].family == BBOLD10  /*textmode && bb text */
                       && symdefs[idef].handler == NULL)
                    || (fontnum == 4 && symdefs[idef].family == CMMIB10 /*textmode && bf text */
                        && symdefs[idef].handler == NULL))
#endif
                    if ((deflen = strlen(symdefs[idef].symbol)) < minlen) {     /*new best match */
                    bestdef = idef;     /* save index of new best match */
                    if ((minlen = deflen) == symlen) {
                        break;
                    }
                }               /*perfect match, so return with it */
        }
    if (bestdef < 0) {           /* failed to look up symbol */
        if (fontnum != 0) {     /* we're in a restricted font mode */
            int oldfontnum = fontnum;   /* save current font family */
            mathchardef *symdef = NULL; /* lookup result with fontnum=0 */
            fontnum = 0;        /*try to look up symbol in any font */
            symdef = get_symdef(symbol);        /* repeat lookup with fontnum=0 */
            fontnum = oldfontnum;       /* reset font family */
            return symdef;
        }                       /* caller gets fontnum=0 lookup */
    }
  end_of_job:
    if (msgfp != NULL && msglevel >= 999) {     /* debugging output */
        fprintf(msgfp,
                "get_symdef> symbol=%s matches symtable[%d]=%s (isligature=%d)\n",
                symbol, bestdef,
                (bestdef < 0 ? "NotFound" : symdefs[bestdef].symbol),
                isligature);
        fflush(msgfp);
    }
    return ((bestdef < 0 ? NULL : &(symdefs[bestdef])));
}

/* ==========================================================================
 * Function:    get_ligature ( expression, family )
 * Purpose: returns symtable[] index for ligature
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char *  containing ligature
 *              whose corresponding mathchardef is wanted
 *      family (I)  int containing NOVALUE for any family,
 *              or, e.g., CYR10 for cyrillic, etc.
 * --------------------------------------------------------------------------
 * Returns: ( int )     symtable[] index defining ligature,
 *              or -9999 if no ligature found or for any error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */

static int get_ligature(const char *expression, int family)
{
    mathchardef *symdefs = symtable;    /* table of mathchardefs */
    const char *ligature = expression;  /*- 1*//* expression ptr */
    const char *symbol = NULL;  /* symdefs[idef].symbol */
    int liglen = strlen(ligature);      /* #chars remaining in expression */
    int iscyrfam = (family == CYR10);   /* true for cyrillic families */
    int idef = 0;               /* symdefs[] index */
    int bestdef = -9999;        /*index of longest matching symdef */
    int maxlen = -9999;         /*length of longest matching symdef */

    if (!isstring) {            /* no ligatures in "string" mode */
        for (idef = 0;; idef++) /* until trailer record found */
            if ((symbol = symdefs[idef].symbol) == NULL) {
                break;          /* end-of-table */
            } else {              /* check against caller's ligature */
                int symlen = strlen(symbol);    /* #chars in symbol */
                if ((symlen > 1 || iscyrfam)    /*ligature >1 char long or cyrillic */
                    &&symlen <= liglen  /* and enough remaining chars */
                    && (*symbol != '\\' || iscyrfam)    /* not escaped or cyrillic */
                    &&symdefs[idef].handler == NULL)    /* and not a handler */
                    if (strncmp(ligature, symbol, symlen) == 0) /* found match */
                        if (family < 0  /* no family specifies */
                            || symdefs[idef].family == family)  /* or have correct family */
                            if (symlen > maxlen) {      /* new longest ligature */
                                bestdef = idef; /* save index of new best match */
                                maxlen = symlen;
            }
        }
        if (msgfp != NULL && msglevel >= 999) { /* debugging output */
            fprintf(msgfp,
                    "get_ligature> ligature=%.4s matches symtable[%d]=%s\n",
                    ligature, bestdef,
                    (bestdef < 0 ? "NotFound" : symdefs[bestdef].symbol));
            fflush(msgfp);
        }
    }
    return bestdef;
}

/* ==========================================================================
 * Function:    get_chardef ( symdef, size )
 * Purpose: returns chardef ptr containing data for symdef at given size
 * --------------------------------------------------------------------------
 * Arguments:   symdef (I)  mathchardef *  corresponding to symbol
 *              whose corresponding chardef is wanted
 *      size (I)    int containing 0-5 for desired size
 * --------------------------------------------------------------------------
 * Returns: ( chardef * )   pointer to struct defining symbol at size,
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o if size unavailable, the next-closer-to-normalsize
 *      is returned instead.
 * ======================================================================= */
static chardef *get_chardef(mathchardef * symdef, int size)
{
    fontfamily *fonts = fonttable;      /* table of font families */
    chardef **fontdef;          /*tables for desired font, by size */
    chardef *gfdata = NULL;     /* chardef for symdef,size */
    int ifont;                  /* fonts[] index */
    int family, charnum;        /* indexes retrieved from symdef */
    int sizeinc = 0;            /*+1 or -1 to get closer to normal */
    int normalsize = 2;         /* this size always present */
    int isBig = 0;              /*true if symbol's 1st char is upper */
    const char *symptr = NULL;  /* look for 1st alpha of symbol */

    if (symdef == NULL) {
        goto end_of_job;
    }
/* --- get local copy of indexes from symdef --- */
     family = symdef->family;   /* font family containing symbol */
     charnum = symdef->charnum; /* char# of symbol within font */
/* --- check for supersampling --- */
    if (issupersampling) {      /* check for supersampling fonts */
        if (fonts != ssfonttable) {     /* uh oh--probably internal error */
            fonts = ssfonttable;
        }
    }
    /* force it */
    /* --- check requested size, and set size increment --- */
    if (0 && issupersampling) {
        size = LARGESTSIZE + 1;        /* index 1 past largest size */
    } else {                      /* low pass indexes 0...LARGESTSIZE */
        if (size < 0) {
            size = 0;           /* size was definitely too small */
        }
        if (size > LARGESTSIZE) {
            size = LARGESTSIZE; /* or definitely too large */
        }
        if (size < normalsize) {
            sizeinc = 1;     /*use next larger if size too small */
        }
        if (size > normalsize) {
            sizeinc = -1;     /*or next smaller if size too large */
        }
    }
/* --- check for really big symbol (1st char of symbol name uppercase) --- */
    for (symptr = symdef->symbol; *symptr != '\0'; symptr++) {    /*skip leading \'s */
        if (isalpha(*symptr)) { /* found leading alpha char */
            isBig = isupper(*symptr);   /* is 1st char of name uppercase? */
            if (!isBig          /* 1st char lowercase */
                && strlen(symptr) >= 4) /* but followed by at least 3 chars */
                isBig = !memcmp(symptr, "big\\", 4)     /* isBig if name starts with big\ */
                    ||!memcmp(symptr, "bigg", 4);       /* or with bigg */
            break;
        }
    }

    /* -------------------------------------------------------------------------
       find font family in table of fonts[]
       -------------------------------------------------------------------------- */
    /* --- look up font family --- */
    for (ifont = 0;; ifont++) {   /* until trailer record found */
        if (fonts[ifont].family < 0) {  /* error, no such family */
            if (msgfp != NULL && msglevel >= 99) {      /* emit error */
                fprintf(msgfp,
                        "get_chardef> failed to find font family %d\n",
                        family);
                fflush(msgfp);
            }
            goto end_of_job;
        } else if (fonts[ifont].family == family) {
            break;              /* found font family */
        }
    }
/* --- get local copy of table for this family by size --- */
    fontdef = fonts[ifont].fontdef;     /* font by size */
/* -------------------------------------------------------------------------
get font in desired size, or closest available size, and return symbol
-------------------------------------------------------------------------- */
/* --- get font in desired size --- */
    for (;;) {                  /* find size or closest available */
        if (fontdef[size] != NULL) {
            break;              /* found available size */
        } else if (size == NORMALSIZE || sizeinc == 0) {  /* or must be supersampling */
            if (msgfp != NULL && msglevel >= 99) {      /* emit error */
                fprintf(msgfp,
                        "get_chardef> failed to find font size %d\n",
                        size);
                fflush(msgfp);
            }
            goto end_of_job;
        } else {                   /*bump size 1 closer to NORMALSIZE */
            size += sizeinc;    /* see if adjusted size available */
        }
    }
/* --- ptr to chardef struct --- */
    gfdata = &((fontdef[size])[charnum]);       /*ptr to chardef for symbol in size */
/* -------------------------------------------------------------------------
kludge to tweak CMEX10 (which appears to have incorrect descenders)
-------------------------------------------------------------------------- */
    if (family == CMEX10) {     /* cmex10 needs tweak */
        int height = gfdata->toprow - gfdata->botrow + 1;       /*total height of char */
        gfdata->botrow = (isBig ? (-height / 3) : (-height / 4));
        gfdata->toprow = gfdata->botrow + gfdata->image.height;
    }
/* -------------------------------------------------------------------------
return subraster containing chardef data for symbol in requested size
-------------------------------------------------------------------------- */
  end_of_job:
    if (msgfp != NULL && msglevel >= 999) {
        if (symdef == NULL) {
            fprintf(msgfp, "get_chardef> input symdef==NULL\n");
        } else {
            fprintf(msgfp,
                    "get_chardef> requested symbol=\"%s\" size=%d  %s\n",
                    symdef->symbol, size,
                    (gfdata == NULL ? "FAILED" : "Succeeded"));
        }
        fflush(msgfp);
    }
    return gfdata;
}

/* ==========================================================================
 * Function:    get_charsubraster ( symdef, size )
 * Purpose: returns new subraster ptr containing
 *      data for symdef at given size
 * --------------------------------------------------------------------------
 * Arguments:   symdef (I)  mathchardef *  corresponding to symbol whose
 *              corresponding chardef subraster is wanted
 *      size (I)    int containing 0-5 for desired size
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) pointer to struct defining symbol at size,
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o just wraps a subraster envelope around get_chardef()
 * ======================================================================= */

static subraster *get_charsubraster(mathchardef * symdef, int size)
{
    chardef *gfdata = NULL;     /* chardef struct for symdef,size */
    subraster *sp = NULL;       /* subraster containing gfdata */
    raster *bitmaprp = NULL;    /* convert .gf-format to bitmap */
    int grayscale = 256;        /* aasupersamp() parameters */

/* -------------------------------------------------------------------------
look up chardef for symdef at size, and embed data (gfdata) in subraster
-------------------------------------------------------------------------- */
    if ((gfdata = get_chardef(symdef, size)) != NULL) {
        /* we found it */
        /* allocate subraster "envelope" */
        if ((sp = new_subraster(0, 0, 0)) != NULL) {
            raster *image = &(gfdata->image);   /* ptr to chardef's bitmap or .gf */
            int format = image->format; /* 1=bitmap, else .gf */
            sp->symdef = symdef;       /* replace NULL with caller's arg */
            sp->size = size;   /*replace default with caller's size */
            sp->baseline = get_baseline(gfdata);       /* get baseline of character */

            if (format == 1) {  /* already a bitmap */
                sp->type = CHARASTER;   /* static char raster */
                sp->image = image;
            } else if ((bitmaprp = gftobitmap(image)) != NULL) {
                /* convert successful */
                sp->type = IMAGERASTER; /* allocated raster will be freed */
                sp->image = bitmaprp;
            } else {
                /* conversion failed */
                delete_subraster(sp);
                sp = NULL;        /* signal error to caller */
                goto end_of_job;
            }                   /* quit */
            if (issupersampling) {      /* antialias character right here */
                raster *aa = NULL;      /* antialiased char raster */
                int status = aasupsamp(sp->image, &aa, shrinkfactor, grayscale);
                if (status) {   /* supersampled successfully */
                    int baseline = sp->baseline;        /* baseline before supersampling */
                    int height = gfdata->image.height;  /* #rows before supersampling */
                    sp->image = aa;     /* replace chardef with ss image */
                    if (baseline >= height - 1) { /* baseline at bottom of char */
                        sp->baseline = aa->height - 1;  /* so keep it at bottom */
                    } else {        /* char has descenders */
                        sp->baseline /= shrinkfactor;   /* rescale baseline */
                    }
                    sp->type = IMAGERASTER;
                }               /* character is an image raster */
            }
        }
    }
  end_of_job:
    if (msgfp != NULL && msglevel >= 999) {
        fprintf(msgfp,
                "get_charsubraster> requested symbol=\"%s\" baseline=%d"
                " %s %s\n", symdef->symbol,
                (sp == NULL ? 0 : sp->baseline),
                (sp == NULL ? "FAILED" : "Succeeded"),
                (gfdata == NULL ? "(gfdata=NULL)" : " "));
        fflush(msgfp);
    }
    return sp;
}

/* ==========================================================================
 * Function:    get_symsubraster ( symbol, size )
 * Purpose: returns new subraster ptr containing
 *      data for symbol at given size
 * --------------------------------------------------------------------------
 * Arguments:   symbol (I)  char *  corresponding to symbol
 *              whose corresponding subraster is wanted
 *      size (I)    int containing 0-5 for desired size
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) pointer to struct defining symbol at size,
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o just combines get_symdef() and get_charsubraster()
 * ======================================================================= */

static subraster *get_symsubraster(const char *symbol, int size)
{
    subraster *sp = NULL; /* subraster containing gfdata */
    mathchardef *symdef = NULL;  /* mathchardef lookup for symbol */

    if (symbol != NULL) {        /* user supplied input symbol */
         symdef = get_symdef(symbol);   /*look up corresponding mathchardef */
    }
/* -------------------------------------------------------------------------
look up chardef for mathchardef and wrap a subraster structure around data
-------------------------------------------------------------------------- */
    if (symdef != NULL) {        /* lookup succeeded */
         sp = get_charsubraster(symdef, size);  /* so get symbol data in subraster */
    }
    return sp;
}
/* ==========================================================================
 * Function:    get_baseline ( gfdata )
 * Purpose: returns baseline for a chardef struct
 * --------------------------------------------------------------------------
 * Arguments:   gfdata (I)  chardef *  containing chardef for symbol
 *              whose baseline is wanted
 * --------------------------------------------------------------------------
 * Returns: ( int )     baseline for symdef,
 *              or -1 for any error
 * --------------------------------------------------------------------------
 * Notes:     o Unlike TeX, the top-left corners of our rasters are (0,0),
 *      with (row,col) increasing as you move down and right.
 *      Baselines are calculated with respect to this scheme,
 *      so 0 would mean the very top row is on the baseline
 *      and everything else descends below the baseline.
 * ======================================================================= */
static int get_baseline(chardef * gfdata)
{
    /*toprow = gfdata->toprow, */
    /*TeX top row from .gf file info */
    int botrow = gfdata->botrow;   /*TeX bottom row from .gf file info */
    int height = gfdata->image.height;  /* #rows comprising symbol */

    return (height - 1) + botrow;     /* note: descenders have botrow<0 */
}

/* ==========================================================================
 * Function:    get_delim ( char *symbol, int height, int family )
 * Purpose: returns subraster corresponding to the samllest
 *      character containing symbol, but at least as large as height,
 *      and in caller's family (if specified).
 *      If no symbol character as large as height is available,
 *      then the largest availabale character is returned instead.
 * --------------------------------------------------------------------------
 * Arguments:   symbol (I)  char *  containing (substring of) desired
 *              symbol, e.g., if symbol="(", then any
 *              mathchardef like "(" or "\\(", etc, match.
 *      height (I)  int containing minimum acceptable height
 *              for returned character
 *      family (I)  int containing -1 to consider all families,
 *              or, e.g., CMEX10 for only that family
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) best matching character available,
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o If height is passed as negative, its absolute value is used
 *      but the best-fit width is searched for (rather than height)
 * ======================================================================= */
/* --- entry point --- */
static subraster *get_delim(const char *symbol, int height, int family) {
    mathchardef *symdefs = symtable;    /* table of mathchardefs */
    subraster *sp = NULL;       /* best match char */
    chardef *gfdata = NULL;     /* get chardef struct for a symdef */
    char lcsymbol[256];
    char *symptr;               /* lowercase symbol for comparison */
    const char *unescsymbol = symbol;   /* unescaped symbol */
    int symlen = (symbol == NULL ? 0 : strlen(symbol)); /* #chars in caller's sym */
    int deflen = 0;             /* length of symdef (aka lcsymbol) */
    int idef = 0;               /* symdefs[] index */
    int bestdef = -9999;        /* index of best fit symdef */
    int bigdef = -9999;         /*index of biggest (in case no best) */
    int size = 0;               /* size index 0...LARGESTSIZE */
    int bestsize = -9999;       /* index of best fit size */
    int bigsize = -9999;        /*index of biggest (in case no best) */
    int defheight, bestheight = 9999;   /* height of best fit symdef */
    int bigheight = -9999;      /*height of biggest(in case no best) */
    int iswidth = 0;            /* true if best-fit width desired */
    int isunesc = 0;            /* true if leading escape removed */
    int issq = 0, isoint = 0;   /* true for \sqcup,etc, \oint,etc */
    int iscurly = 0;            /* true for StMary's curly symbols */
    const char *bigint = "bigint";
    const char *bigoint = "bigoint";    /* substitutes for int, oint */
/* -------------------------------------------------------------------------
determine if searching height or width, and search symdefs[] for best-fit
-------------------------------------------------------------------------- */
/* --- arg checks --- */
    if (symlen < 1) {
        return sp;              /* no input symbol suplied */
    }
    if (strcmp(symbol, "e") == 0) {
        return sp;              /* e causes segfault??? */
    }
    if (strstr(symbol, "curly") != NULL) {
        iscurly = 1;            /* user wants curly delim */
    }
/* --- ignore leading escapes for CMEX10 --- */
    if (1) {                     /* ignore leading escape */
        if ((family == CMEX10 || family == CMSYEX)) {   /* for CMEX10 or CMSYEX */
            if (strstr(symbol, "sq") != NULL)   /* \sq symbol requested */
                issq = 1;       /* seq \sq signal */
            if (strstr(symbol, "oint") != NULL) { /* \oint symbol requested */
                isoint = 1;     /* seq \oint signal */
            }
            if (*symbol == '\\') {      /* have leading \ */
                unescsymbol = symbol + 1;       /* push past leading \ */
                if (--symlen < 1) {
                    return sp;        /* one less char */
                }
                if (strcmp(unescsymbol, "int") == 0) {    /* \int requested by caller */
                    unescsymbol = bigint;       /* but big version looks better */
                }
                if (strcmp(unescsymbol, "oint") == 0) {   /* \oint requested by caller */
                    unescsymbol = bigoint;      /* but big version looks better */
                }
                symlen = strlen(unescsymbol);   /* explicitly recalculate length */
                isunesc = 1;
            }                   /* signal leading escape removed */
        }
    }
    /* --- determine whether searching for best-fit height or width --- */
    if (height < 0) {           /* negative signals width search */
        height = -height;       /* flip "height" positive */
        iswidth = 1;
    }
    /* set flag for width search */
    /* --- search symdefs[] for best-fit height (or width) --- */
    for (idef = 0;; idef++) {   /* until trailer record found */
        const char *defsym = symdefs[idef].symbol;      /* local copies */
        int deffam = symdefs[idef].family;
        if (defsym == NULL) {
            break;              /* reached end-of-table */
        } else if (family < 0 || deffam == family     /* if explicitly in caller's family */
                 || (family == CMSYEX && (deffam == CMSY10 || deffam == CMEX10 || deffam == STMARY10))) {
            strcpy(lcsymbol, defsym);   /* local copy of symdefs[] symbol */
            if (isunesc && *lcsymbol == '\\') { /* ignored leading \ in symbol */
                strsqueeze(lcsymbol, 1);
            }                   /*so squeeze it out of lcsymbol too */
            if (0) {              /* don't ignore case */
                for (symptr = lcsymbol; *symptr != '\0'; symptr++) {     /*for each symbol ch */
                    if (isalpha(*symptr)) {
                        *symptr = tolower(*symptr);
                    }
                }
            }
            deflen = strlen(lcsymbol);  /* #chars in symbol we're checking */
            if ((symptr = strstr(lcsymbol, unescsymbol)) != NULL)       /*found caller's sym */
                if ((isoint || strstr(lcsymbol, "oint") == NULL)        /* skip unwanted "oint" */
                    &&(issq || strstr(lcsymbol, "sq") == NULL)) /* skip unwanted "sq" */
                    if ((deffam == CMSY10 ?     /* CMSY10 or not CMSY10 */
                         symptr == lcsymbol     /* caller's sym is a prefix */
                         && deflen == symlen :  /* and same length */
                         (iscurly || strstr(lcsymbol, "curly") == NULL) &&      /*not unwanted curly */
                         (symptr == lcsymbol    /* caller's sym is a prefix */
                          || symptr == lcsymbol + deflen - symlen)))    /* or a suffix */
                        for (size = 0; size <= LARGESTSIZE; size++)     /* check all font sizes */
                            if ((gfdata = get_chardef(&(symdefs[idef]), size)) != NULL) {       /*got one */
                                defheight = gfdata->image.height;       /* height of this character */
                                if (iswidth)    /* width search wanted instead... */
                                    defheight = gfdata->image.width;    /* ...so substitute width */
                                leftsymdef = &(symdefs[idef]);  /* set symbol class, etc */
                                if (defheight >= height && defheight < bestheight) {    /*new best fit */
                                    bestdef = idef;
                                    bestsize = size;    /* save indexes of best fit */
                                    bestheight = defheight;
                                }       /* and save new best height */
                                if (defheight >= bigheight) {   /* new biggest character */
                                    bigdef = idef;
                                    bigsize = size;     /* save indexes of biggest */
                                    bigheight = defheight;
                                }       /* and save new big height */
                            }
        }
    }
/* -------------------------------------------------------------------------
construct subraster for best fit character, and return it to caller
-------------------------------------------------------------------------- */
    if (bestdef >= 0) {          /* found a best fit for caller */
        sp = get_charsubraster(&(symdefs[bestdef]), bestsize);  /* best subraster */
    }
    if ((sp == NULL && height - bigheight > 5) ||bigdef < 0) {           /* delim not in font tables */
        sp = make_delim(symbol, (iswidth ? -height : height));  /* try to build delim */
    }
    if (sp == NULL && bigdef >= 0) {     /* just give biggest to caller */
        sp = get_charsubraster(&(symdefs[bigdef]), bigsize);    /* biggest subraster */
    }
    if (msgfp != NULL && msglevel >= 99) {
        fprintf(msgfp,
                "get_delim> symbol=%.50s, height=%d family=%d isokay=%s\n",
                (symbol == NULL ? "null" : symbol), height, family,
                (sp == NULL ? "fail" : "success"));
    }
    return sp;
}

/* ==========================================================================
 * Function:    make_delim ( char *symbol, int height )
 * Purpose: constructs subraster corresponding to symbol
 *      exactly as large as height,
 * --------------------------------------------------------------------------
 * Arguments:   symbol (I)  char *  containing, e.g., if symbol="("
 *              for desired delimiter
 *      height (I)  int containing height
 *              for returned character
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) constructed delimiter
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o If height is passed as negative, its absolute value is used
 *      and interpreted as width (rather than height)
 * ======================================================================= */
 
subraster *make_delim(const char *symbol, int height)
{
    subraster *sp = NULL;       /* subraster returned to caller */
    subraster *symtop = NULL, *symbot = NULL, *symmid = NULL, *symbar = NULL;   /* pieces */
    subraster *topsym = NULL, *botsym = NULL, *midsym = NULL, *barsym = NULL;   /* +filler */
    int isdrawparen = 0;        /*1=draw paren, 0=build from pieces */
    raster *rasp = NULL;        /* sp->image */
    int isokay = 0;             /* set true if delimiter drawn ok */
    int pixsz = 1;              /* pixels are one bit each */
    int symsize = 0;            /* size arg for get_symsubraster() */
    int thickness = 1;          /* drawn lines are one pixel thick */
    int aspectratio = 8;        /* default height/width for parens */
    int iswidth = 0;            /*true if width specified by height */
    int width = height;         /* #pixels width (e.g., of ellipse) */
    char *lp = NULL, *rp = NULL;        /* check symbol for left or right */
    char *lp2 = NULL, *rp2 = NULL;       /* synonym for lp,rp */
    char *lp3 = NULL, *rp3 = NULL;       /* synonym for lp,rp */
    char *lp4 = NULL, *rp4 = NULL;       /* synonym for lp,rp */
    int isprealloc = 1;         /*pre-alloc subraster, except arrow */
    int oldsmashmargin = smashmargin;   /* save original smashmargin */
    int wasnocatspace = isnocatspace;   /* save original isnocatspace */

/* --- determine whether constructing height or width --- */
    if (height < 0) {           /* negative "height" signals width */
        width = height = -height;       /* flip height positive */
        iswidth = 1;
    }                           /* set flag for width */
    if (height < 3) {
        goto end_of_job;        /* too small, must be error */
    }
/* --- set default width (or height) accordingly --- */
    if (iswidth) {
        height = (width + (aspectratio + 1) / 2) / aspectratio;
    } else {
        width = (height + (aspectratio + 1) / 2) / aspectratio;
    }
    if (strchr(symbol, '=') != NULL     /* left or right || bracket wanted */
        || strstr(symbol, "\\|") != NULL        /* same || in standard tex notation */
        || strstr(symbol, "dbl") != NULL)       /* semantic bracket with ||'s */
        width = max2(width, 6); /* need space between two |'s */
    if (width < 2) {
        width = 2;              /* set min width */
    }
    if (strchr(symbol, '(') != NULL  || strchr(symbol, ')') != NULL) {
        width = (3 * width) / 2;        /* adjust width */
        if (!isdrawparen) {
            isprealloc = 0;
        }
    }                           /* don't prealloc if building */
    if (strchr(symbol, '/') != NULL     /* left / */
        || strstr(symbol, "\\\\") != NULL       /* or \\ for right \ */
        || strstr(symbol, "backsl") != NULL)    /* or \backslash for \ */
        width = max2(height / 3, 5);
    if (strstr(symbol, "arrow") != NULL) {      /* arrow wanted */
        width = min2(height / 3, 20);   /* adjust width */
        isprealloc = 0;
    }                           /* don't preallocate subraster */
    if (strchr(symbol, '{') != NULL     /* if left { */
        || strchr(symbol, '}') != NULL) {       /* or right } brace wanted */
        isprealloc = 0;
    }
    /* don't preallocate */
    /* --- allocate and initialize subraster for constructed delimiter --- */
    if (isprealloc) {           /* pre-allocation wanted */
        if ((sp = new_subraster(width, height, pixsz)) == NULL) {
            goto end_of_job;
        }
        /* --- initialize delimiter subraster parameters --- */
        sp->type = IMAGERASTER; /* image */
        sp->symdef = NULL;      /* not applicable for image */
        sp->baseline = height / 2 + 2;  /* is a little above center good? */
        sp->size = NORMALSIZE;  /* size (probably unneeded) */
        rasp = sp->image;
    }

    /* -------------------------------------------------------------------------
       ( ) parens
       -------------------------------------------------------------------------- */
    if ((lp = strchr(symbol, '(')) != NULL  || (rp = strchr(symbol, ')')) != NULL) {
        if (isdrawparen) {      /* draw the paren */
            int mywidth = min2(width, 20);      /* max width for ()'s */
            circle_raster(rasp, /* embedded raster image */
                          0, 0, /* row0,col0 are upper-left corner */
                          height - 1, mywidth - 1,      /* row1,col1 are lower-right */
                          thickness,    /* line thickness is 1 pixel */
                          (rp == NULL ? "23" : "41"));  /* "1234" quadrants to be drawn */
            isokay = 1;
        } else {
            int isleft = (lp != NULL ? 1 : 0);  /* true for left, false for right */
            const char *parentop = (isleft ? "\\leftparentop" : "\\rightparentop");
            const char *parenbot = (isleft ? "\\leftparenbot" : "\\rightparenbot");
            const char *parenbar = (isleft ? "\\leftparenbar" : "\\rightparenbar");
            int baseht = 0, barht = 0;  /* height of base=top+bot, bar */
            int ibar = 0, nbars = 0;    /* bar index, #bars between top&bot */
            int largestsize = min2(2, LARGESTSIZE);     /* largest size for parens */
            int topfill = (isleft ? 0 : 0);
            int botfill = (isleft ? 0 : 0);
            int barfill = (isleft ? 0 : 7);     /* alignment fillers */
            /* --- get pieces at largest size smaller than total height --- */
            for (symsize = largestsize; symsize >= 0; symsize--) {      /*largest to smallest */
                /* --- get pieces at current test size --- */
                isokay = 1;     /* check for all pieces */
                if ((symtop = get_symsubraster(parentop, symsize)) == NULL) {
                    isokay = 0;
                }
                if ((symbot = get_symsubraster(parenbot, symsize)) == NULL) {
                    isokay = 0;
                }
                if ((symbar = get_symsubraster(parenbar, symsize)) == NULL) {
                    isokay = 0;
                }
                /* --- check sum of pieces against total desired height --- */
                if (isokay) {   /* all pieces retrieved */
                    baseht = (symtop->image)->height + (symbot->image)->height; /*top+bot */
                    barht = (symbar->image)->height;    /* bar height */
                    if (baseht < height + 5) {
                        break;  /* largest base that's not too big */
                    }
                    if (symsize < 1) {
                        break;  /* or smallest available base */
                    }
                }

                /* --- free test pieces that were too big --- */
                if (symtop != NULL) {
                    delete_subraster(symtop);   /* free top */
                }
                if (symbot != NULL) {
                    delete_subraster(symbot);   /* free bot */
                }
                if (symbar != NULL) {
                    delete_subraster(symbar);   /* free bar */
                }
                isokay = 0;     /* nothing available */
                if (symsize < 1) {
                    break;      /* leave isokay=0 after smallest */
                }
            }

            /* --- construct brace from pieces --- */
            if (isokay) {       /* we have the pieces */
                /* --- add alignment fillers --- */
                smashmargin = 0;
                isnocatspace = 99;      /*turn off rastcat smashing,space */

                topsym = (topfill > 0 ? rastcat(new_subraster(topfill, 1, 1), symtop, 3) : symtop);
                botsym = (botfill > 0 ? rastcat(new_subraster(botfill, 1, 1), symbot, 3) : symbot);
                barsym = (barfill > 0 ? rastcat(new_subraster(barfill, 1, 1), symbar, 3) : symbar);

                smashmargin = oldsmashmargin;   /* reset smashmargin */
                isnocatspace = wasnocatspace;   /* reset isnocatspace */
                /* --- #bars needed between top and bot --- */
                nbars = (barht < 1 ? 0 : max2(0, 1 + (height - baseht) / barht));       /* #bars needed */
                /* --- stack pieces --- */
                sp = topsym;    /* start with top piece */
                if (nbars > 0)  /* need nbars between top and bot */
                    for (ibar = 1; ibar <= nbars; ibar++)
                        sp = rastack(barsym, sp, 1, 0, 0, 2);
                sp = rastack(botsym, sp, 1, 0, 0, 3);   /* bottom below bars or middle */
                delete_subraster(barsym);       /* barsym no longer needed */
            }
        }
    } else if ((lp = strchr(symbol, '{')) != NULL || (rp = strchr(symbol, '}')) != NULL) {
        int isleft = (lp != NULL ? 1 : 0);      /* true for left, false for right */
        const char *bracetop =(isleft ? "\\leftbracetop" : "\\rightbracetop");
        const char *bracebot =(isleft ? "\\leftbracebot" : "\\rightbracebot");
        const char *bracemid =(isleft ? "\\leftbracemid" : "\\rightbracemid");
        const char *bracebar =(isleft ? "\\leftbracebar" : "\\rightbracebar");
        int baseht = 0, barht = 0;      /* height of base=top+bot+mid, bar */
        int ibar = 0, nbars = 0;        /* bar index, #bars above,below mid */
        int largestsize = min2(2, LARGESTSIZE); /* largest size for braces */
        int topfill = (isleft ? 4 : 0);
        int botfill = (isleft ? 4 : 0);
        int midfill = (isleft ? 0 : 4);
        int barfill = (isleft ? 4 : 4); /* alignment fillers */

        /* --- get pieces at largest size smaller than total height --- */
        for (symsize = largestsize; symsize >= 0; symsize--) {  /*largest to smallest */
            /* --- get pieces at current test size --- */
            isokay = 1;         /* check for all pieces */
            if ((symtop = get_symsubraster(bracetop, symsize)) == NULL) {
                isokay = 0;
            }
            if ((symbot = get_symsubraster(bracebot, symsize)) == NULL) {
                isokay = 0;
            }
            if ((symmid = get_symsubraster(bracemid, symsize)) == NULL) {
                isokay = 0;
            }
            if ((symbar = get_symsubraster(bracebar, symsize)) == NULL) {
                isokay = 0;
            }
            /* --- check sum of pieces against total desired height --- */
            if (isokay) {       /* all pieces retrieved */
                baseht = symtop->image->height + symbot->image->height + symmid->image->height;   /* top+bot+mid height */
                barht = symbar->image->height;        /* bar height */
                if (baseht < height + 5) {
                    break;      /* largest base that's not too big */
                }
                if (symsize < 1) {
                    break;      /* or smallest available base */
                }
            }
            /* --- free test pieces that were too big --- */
            if (symtop != NULL) {
                delete_subraster(symtop);       /* free top */
            }
            if (symbot != NULL) {
                delete_subraster(symbot);       /* free bot */
            }
            if (symmid != NULL) {
                delete_subraster(symmid);       /* free mid */
            }
            if (symbar != NULL) {
                delete_subraster(symbar);       /* free bar */
            }
            isokay = 0;         /* nothing available */
            if (symsize < 1) {
                break;          /* leave isokay=0 after smallest */
            }
        }
        /* --- construct brace from pieces --- */
        if (isokay) {           /* we have the pieces */
            /* --- add alignment fillers --- */
            smashmargin = 0;
            isnocatspace = 99;  /*turn off rastcat smashing,space */
            topsym = (topfill > 0 ? rastcat(new_subraster(topfill, 1, 1), symtop, 3) : symtop);
            botsym = (botfill > 0 ? rastcat(new_subraster(botfill, 1, 1), symbot, 3) : symbot);
            midsym = (midfill > 0 ? rastcat(new_subraster(midfill, 1, 1), symmid, 3) : symmid);
            barsym = (barfill > 0 ? rastcat(new_subraster(barfill, 1, 1), symbar, 3) : symbar);

            smashmargin = oldsmashmargin;       /* reset smashmargin */
            isnocatspace = wasnocatspace;       /* reset isnocatspace */
            /* --- #bars needed on each side of mid piece --- */
            nbars = (barht < 1 ? 0 : max2(0, 1 + (height - baseht) / barht / 2));       /*#bars per side */
            /* --- stack pieces --- */
            sp = topsym;        /* start with top piece */
            if (nbars > 0) {      /* need nbars above middle */
                for (ibar = 1; ibar <= nbars; ibar++) {
                    sp = rastack(barsym, sp, 1, 0, 0, 2);
                }
            }
            sp = rastack(midsym, sp, 1, 0, 0, 3);       /*mid after top or bars */
            if (nbars > 0) {      /* need nbars below middle */
                for (ibar = 1; ibar <= nbars; ibar++) {
                    sp = rastack(barsym, sp, 1, 0, 0, 2);
                }
            }
            sp = rastack(botsym, sp, 1, 0, 0, 3);       /* bottom below bars or middle */
            delete_subraster(barsym);   /* barsym no longer needed */
        }
    } else if ((lp = strchr(symbol, '[')) != NULL
             || (rp = strchr(symbol, ']')) != NULL
             || (lp2 = strstr(symbol, "lceil")) != NULL /* left ceiling wanted */
             || (rp2 = strstr(symbol, "rceil")) != NULL /* right ceiling wanted */
             || (lp3 = strstr(symbol, "lfloor")) != NULL        /* left floor wanted */
             || (rp3 = strstr(symbol, "rfloor")) != NULL        /* right floor wanted */
             || (lp4 = strstr(symbol, "llbrack")) != NULL       /* left semantic bracket */
             || (rp4 = strstr(symbol, "rrbrack")) != NULL) {    /* right semantic bracket */

        /* --- use rule_raster ( rasp, top, left, width, height, type=0 ) --- */
        int mywidth = min2(width, 12);   /* max width for horizontal bars */
        int wthick = 1;         /* thickness of top.bottom bars */
        thickness = (height < 25 ? 1 : 2);      /* set lines 1 or 2 pixels thick */

        if (lp2 != NULL || rp2 != NULL || lp3 != NULL || rp3 != NULL) {   /*ceil or floor */
            wthick = thickness; /* same thickness for top/bot bar */
        }
        if (lp3 == NULL && rp3 == NULL) { /* set top bar if floor not wanted */
            rule_raster(rasp, 0, 0, mywidth, wthick, 0);        /* top horizontal bar */
        }
        if (lp2 == NULL && rp2 == NULL) { /* set bot bar if ceil not wanted */
            rule_raster(rasp, height - wthick, 0, mywidth, thickness, 0);       /* bottom */
        }
        if (lp != NULL || lp2 != NULL || lp3 != NULL || lp4 != NULL) {
            rule_raster(rasp, 0, 0, thickness, height, 0);      /* left vertical bar */
        }
        if (lp4 != NULL) {
            rule_raster(rasp, 0, thickness + 1, 1, height, 0);  /* 2nd left vertical bar */
        }
        if (rp != NULL || rp2 != NULL || rp3 != NULL || rp4 != NULL) {   /* right bracket */
            rule_raster(rasp, 0, mywidth - thickness, thickness, height, 0);    /* right */
        }
        if (rp4 != NULL) {
            rule_raster(rasp, 0, mywidth - thickness - 2, 1, height, 0);        /*2nd right vert */
        }
        isokay = 1;
    } else if ((lp = strchr(symbol, '<')) != NULL || (rp = strchr(symbol, '>')) != NULL) {
        /* --- use line_raster( rasp,  row0, col0,  row1, col1,  thickness ) --- */
        int mywidth = min2(width, 12);  /* max width for brackets */
        int mythick = 1;        /* all lines one pixel thick */
        thickness = (height < 25 ? 1 : 2);      /* set line pixel thickness */
        if (lp != NULL) {       /* left < bracket wanted */
            line_raster(rasp, height / 2, 0, 0, mywidth - 1, mythick);
            if (thickness > 1) {
                line_raster(rasp, height / 2, 1, 0, mywidth - 1, mythick);
            }
            line_raster(rasp, height / 2, 0, height - 1, mywidth - 1, mythick);
            if (thickness > 1) {
                line_raster(rasp, height / 2, 1, height - 1, mywidth - 1, mythick);
            }
        }
        if (rp != NULL) {       /* right > bracket wanted */
            line_raster(rasp, height / 2, mywidth - 1, 0, 0, mythick);
            if (thickness > 1) {
                line_raster(rasp, height / 2, mywidth - 2, 0, 0, mythick);
            }
            line_raster(rasp, height / 2, mywidth - 1, height - 1, 0, mythick);
            if (thickness > 1) {
                line_raster(rasp, height / 2, mywidth - 2, height - 1, 0, mythick);
            }
        }
        isokay = 1;
    } else if ((lp = strchr(symbol, '/')) != NULL /* left /  wanted */
             || (rp = strstr(symbol, "\\\\")) != NULL   /* right \ wanted */
             || (rp2 = strstr(symbol, "backsl")) != NULL) {     /* right \ wanted */
        /* --- use line_raster( rasp,  row0, col0,  row1, col1,  thickness ) --- */
        int mywidth = width;    /* max width for / \ */
        thickness = 1;          /* set line pixel thickness */
        if (lp != NULL)         /* left / wanted */
            line_raster(rasp, 0, mywidth - 1, height - 1, 0, thickness);
        if (rp != NULL || rp2 != NULL)  /* right \ wanted */
            line_raster(rasp, 0, 0, height - 1, mywidth - 1, thickness);
        isokay = 1;             /* set flag */
    } else if (strstr(symbol, "arrow") != NULL) { /* arrow delimiter wanted */
        /* --- use uparrow_subraster(width,height,pixsz,drctn,isBig) --- */
        int mywidth = width;    /* max width for / \ */
        int isBig = (strstr(symbol, "Up") != NULL       /* isBig if we have an Up */
                     || strstr(symbol, "Down") != NULL);        /* or a Down */
        int drctn = +1;         /* init for uparrow */
        if (strstr(symbol, "down") != NULL      /* down if we have down */
            || strstr(symbol, "Down") != NULL) {        /* or Down */
            drctn = (-1);       /* reset direction to down */
            if (strstr(symbol, "up") != NULL    /* updown if we have up or Up */
                || strstr(symbol, "Up") != NULL)        /* and down or Down */
                drctn = 0;
        }                       /* reset direction to updown */
        sp = uparrow_subraster(mywidth, height, pixsz, drctn, isBig);
        if (sp != NULL) {
            sp->type = IMAGERASTER;     /* image */
            sp->symdef = NULL;  /* not applicable for image */
            sp->baseline = height / 2 + 2;      /* is a little above center good? */
            sp->size = NORMALSIZE;      /* size (probably unneeded) */
            isokay = 1;
        }
    } else if ((lp = strchr(symbol, '-')) != NULL /* left or right | bracket wanted */
             || (lp2 = strchr(symbol, '|')) != NULL     /* synonym for | bracket */
             || (rp = strchr(symbol, '=')) != NULL      /* left or right || bracket wanted */
             || (rp2 = strstr(symbol, "\\|")) != NULL) {        /* || in standard tex notation */
        /* --- rule_raster ( rasp, top, left, width, height, type=0 ) --- */
        int midcol = width / 2; /* middle col, left of mid if even */
        if (rp != NULL          /* left or right || bracket wanted */
            || rp2 != NULL) {   /* or || in standard tex notation */
            thickness = (height < 75 ? 1 : 2);  /* each | of || 1 or 2 pixels thick */
            rule_raster(rasp, 0, max2(0, midcol - 2), thickness, height, 0);    /* left */
            rule_raster(rasp, 0, min2(width, midcol + 2), thickness,
                        height, 0);
        } else if (lp != NULL    /* left or right | bracket wanted */
                                                          || lp2 != NULL) {     /* ditto for synomym */
            thickness = (height < 75 ? 1 : 2);  /* set | 1 or 2 pixels thick */
            rule_raster(rasp, 0, midcol, thickness, height, 0);
        }                       /*mid vertical bar */
        isokay = 1;
    }

  end_of_job:
    if (msgfp != NULL && msglevel >= 99) {
        fprintf(msgfp, "make_delim> symbol=%.50s, isokay=%d\n",
                (symbol == NULL ? "null" : symbol), isokay);
    }
    if (!isokay) {              /* don't have requested delimiter */
        if (sp != NULL) {
            delete_subraster(sp);       /* so free unneeded structure */
        }
        sp = NULL;
    }                           /* and signal error to caller */
    return sp;
}

/* ==========================================================================
 * Function:    texchar ( expression, chartoken )
 * Purpose: scans expression, returning either its first character,
 *      or the next \sequence if that first char is \,
 *      and a pointer to the first expression char past that.
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char * to first char of null-terminated
 *              string containing valid LaTeX expression
 *              to be scanned
 *      chartoken (O)   char * to null-terminated string returning
 *              either the first (non-whitespace) character
 *              of expression if that char isn't \, or else
 *              the \ and everything following it up to
 *              the next non-alphabetic character (but at
 *              least one char following the \ even if
 *              it's non-alpha)
 * --------------------------------------------------------------------------
 * Returns: ( char * )  ptr to the first char of expression
 *              past returned chartoken,
 *              or NULL for any parsing error.
 * --------------------------------------------------------------------------
 * Notes:     o Does *not* skip leading whitespace, but simply
 *      returns any whitespace character as the next character.
 * ======================================================================= */

char *texchar(const char *expression, char *chartoken)
{
    int esclen = 0;             /*length of escape sequence */
    int maxesclen = 128;        /* max len of esc sequence */
    char *ptoken = chartoken;   /* ptr into chartoken */
    int iprefix = 0;            /* prefix index */
    static const char *prefixes[] =     /*e.g., \big followed by ( */
        {"\\big", "\\Big", "\\bigg", "\\Bigg",
         "\\bigl", "\\Bigl", "\\biggl", "\\Biggl",
         "\\bigr", "\\Bigr", "\\biggr", "\\Biggr", NULL};
    static const char *starred[] = {"\\hspace", "\\!", NULL};

/* -------------------------------------------------------------------------
just return the next char if it's not \
-------------------------------------------------------------------------- */
    *ptoken = '\0';             /* init in case of error */
    if (expression == NULL) {
        return NULL;            /* nothing to scan */
    }
    if (*expression == '\0') {
        return NULL;            /* nothing to scan */
    }
/* --- always returning first character (either \ or some other char) --- */
    *ptoken++ = *expression++;  /* here's first character */
/* --- if first char isn't \, then just return it to caller --- */
    if (!isthischar(*(expression - 1), ESCAPE)) {       /* not a \, so return char */
        *ptoken = '\0';         /* add a null terminator */
        goto end_of_job;
    }                           /* ptr past returned char */
    if (*expression == '\0') {  /* \ is very last char */
        *chartoken = '\0';      /* flush bad trailing \ */
        return NULL;
    }

    /* -------------------------------------------------------------------------
       we have an escape sequence, so return all alpha chars following \
       -------------------------------------------------------------------------- */
    /* --- accumulate chars until first non-alpha char found --- */
    for (; isalpha(*expression); esclen++) {    /* till first non-alpha... */
        if (esclen < maxesclen - 3)     /* more room in chartoken */
            *ptoken++ = *expression;    /*copy alpha char, bump ptr */
        expression++;
    }                           /* bump expression ptr */
/* --- if we have a prefix, append next texchar, e.g., \big( --- */
    *ptoken = '\0';             /* set null for compare */
    for (iprefix = 0; prefixes[iprefix] != NULL; iprefix++) {    /* run thru list */
        if (strcmp(chartoken, prefixes[iprefix]) == 0) {        /* have an exact match */
            char nextchar[256];
            int nextlen = 0;    /* texchar after prefix */
            skipwhite(expression);      /* skip space after prefix */
            expression = texchar(expression, nextchar); /* get nextchar */
            if ((nextlen = strlen(nextchar)) > 0) {     /* #chars in nextchar */
                strcpy(ptoken, nextchar);       /* append nextchar */
                ptoken += strlen(nextchar);     /* point to null terminator */
                esclen += strlen(nextchar);
            }                   /* and bump escape length */
            break;
        }
    }
    /* stop checking prefixes */
    /* --- every \ must be followed by at least one char, e.g., \[ --- */
    if (esclen < 1) {           /* \ followed by non-alpha */
        *ptoken++ = *expression++;      /*copy non-alpha, bump ptrs */
    }
    *ptoken = '\0';
/* --- check for \hspace* or other starred commands --- */
    for (iprefix = 0; starred[iprefix] != NULL; iprefix++) {    /* run thru list */
        if (strcmp(chartoken, starred[iprefix]) == 0) { /* have an exact match */
            if (*expression == '*') {   /* follows by a * */
                *ptoken++ = *expression++;      /* copy * and bump ptr */
                *ptoken = '\0'; /* null-terminate token */
                break;
            }
        }
    }
    /* stop checking */
    /* --- respect spaces in text mode, except first space after \escape --- */
    if (esclen >= 1) {          /*only for alpha \sequences */
        if (istextmode) {       /* in \rm or \it text mode */
            if (isthischar(*expression, WHITEDELIM)) {  /* delim follows \sequence */
                expression++;
            }
        }
    }

  end_of_job:
    if (msgfp != NULL && msglevel >= 999) {
        fprintf(msgfp, "texchar> returning token = \"%s\"\n", chartoken);
        fflush(msgfp);
    }
    return (char *) expression; /*ptr to 1st non-alpha char */
}

/* ==========================================================================
 * Function:    texsubexpr (expression,subexpr,maxsubsz,
 *      left,right,isescape,isdelim)
 * Purpose: scans expression, returning everything between a balanced
 *      left{...right} subexpression if the first non-whitespace
 *      char of expression is an (escaped or unescaped) left{,
 *      or just the next texchar() otherwise,
 *      and a pointer to the first expression char past that.
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char * to first char of null-terminated
 *              string containing valid LaTeX expression
 *              to be scanned
 *      subexpr (O) char * to null-terminated string returning
 *              either everything between a balanced {...}
 *              subexpression if the first char is {,
 *              or the next texchar() otherwise.
 *      maxsubsz (I)    int containing max #bytes returned
 *              in subexpr buffer (0 means unlimited)
 *      left (I)    char * specifying allowable left delimiters
 *              that begin subexpression, e.g., "{[(<"
 *      right (I)   char * specifying matching right delimiters
 *              in the same order as left, e.g., "}])>"
 *      isescape (I)    int controlling whether escaped and/or
 *              unescaped left,right are matched;
 *              see isbrace() comments below for details.
 *      isdelim (I) int containing true (non-zero) to return
 *              the leading left and trailing right delims
 *              (if any were found) along with subexpr,
 *              or containing false=0 to return subexpr
 *              without its delimiters
 * --------------------------------------------------------------------------
 * Returns: ( char * )  ptr to the first char of expression
 *              past returned subexpr (see Notes),
 *              or NULL for any parsing error.
 * --------------------------------------------------------------------------
 * Notes:     o If subexpr is of the form left{...right},
 *      the outer {}'s are returned as part of subexpr
 *      if isdelim is true; if isdelim is false the {}'s aren't
 *      returned.  In either case the returned pointer is
 *      *always* bumped past the closing right}, even if
 *      that closing right} isn't returned in subexpr.
 *        o If subexpr is not of the form left{...right},
 *      the returned pointer is on the character immediately
 *      following the last character returned in subexpr
 *        o \. acts as LaTeX \right. and matches any \left(
 *      And it also acts as a LaTeX \left. and matches any \right)
 * ======================================================================= */
char *texsubexpr(const char *expression, char *subexpr, int maxsubsz,
                 const char *left, const char *right, int isescape,
                 int isdelim)
{
    char *leftptr, leftdelim[256] = "(\0";    /* left( found in expression */
    char rightdelim[256] = ")\000";      /* and matching right) */
    const char *origexpression = expression;
    char *origsubexpr = subexpr;        /*original inputs */
    int gotescape = 0;          /* true if leading char of expression is \ */
    int prevescape = 0;         /* while parsing, true if preceding char \ */
    int isanyright = 1;         /* true matches any right with left, (...] */
    int isleftdot = 0;          /* true if left brace is a \. */
    int nestlevel = 1;          /* current # of nested braces */
    int subsz = 0 /*,maxsubsz=MAXSUBXSZ */ ;    /*#chars in returned subexpr buffer */
/* -------------------------------------------------------------------------
skip leading whitespace and just return the next char if it's not {
-------------------------------------------------------------------------- */
/* --- skip leading whitespace and error check for end-of-string --- */
    *subexpr = '\0';            /* init in case of error */
    if (expression == NULL) {
        return NULL;            /*can't dereference null ptr */
    }
    skipwhite(expression);      /* leading whitespace gone */
    if (*expression == '\0') {
        return NULL;            /* nothing left to scan */
    }
/* --- set maxsubsz --- */
    if (maxsubsz < 1) {
        maxsubsz = MAXSUBXSZ - 2;       /* input 0 means unlimited */
    }
/* --- check for escape --- */
    if (isthischar(*expression, ESCAPE)) {      /* expression is escaped */
        gotescape = 1;          /* so set flag accordingly */
    }
/* --- check for \left...\right --- */
    if (gotescape) {             /* begins with \ */
        if (memcmp(expression + 1, "left", 4)) {  /* and followed by left */
            if (strchr(left, 'l') != NULL) {      /* caller wants \left's */
                if (strtexchr(expression, "\\left") == expression) {    /*expression=\left... */
                    const char *pright = texleft(expression, subexpr, maxsubsz, /* find ...\right */
                                                 (isdelim ? NULL :
                                                  leftdelim), rightdelim);
                    if (isdelim) {
                        strcat(subexpr, rightdelim);    /* caller wants delims */
                    }
                    return (char *) pright;     /*back to caller past \right */
                }
            }
        }
    }

    /* --- if first char isn't left{ or script, just return it to caller --- */
    if (!isbrace(expression, left, isescape)) {
        /* not a left{ */
        if (!isthischar(*expression, SCRIPTS)) {        /* and not a script */
            return texchar(expression, subexpr);        /* next char to caller */
        } else {                /* --- kludge for super/subscripts to accommodate texscripts() --- */
            *subexpr++ = *expression;   /* signal script */
            *subexpr = '\0';
            return (char *) expression;
        }
    }
    /* leave script in stream */
    /* --- extract left and find matching right delimiter --- */
    *leftdelim = *(expression + gotescape);     /* the left( in expression */
    if ((gotescape && *leftdelim == '.')        /* we have a left \. */
        ||(gotescape && isanyright)) {  /*or are matching any right */
        isleftdot = 1;          /* so just set flag */
        *leftdelim = '\0';
    } else if ((leftptr = strchr(left, *leftdelim)) != NULL) {  /* ptr to that left( */
        *rightdelim = right[(int) (leftptr - left)];    /* get the matching right) */
    } else {                    /* can't happen -- pgm bug */
        return NULL;            /*just signal eoj to caller */
    }
/* -------------------------------------------------------------------------
accumulate chars between balanced {}'s, i.e., till nestlevel returns to 0
-------------------------------------------------------------------------- */
/* --- first initialize by bumping past left{ or \{ --- */
    if (isdelim) {
        *subexpr++ = *expression++;     /*caller wants { in subexpr */
    } else {
        expression++;           /* always bump past left{ */
    }
    if (gotescape) {            /*need to bump another char */
        if (isdelim) {
            *subexpr++ = *expression++; /* caller wants char, too */
        } else {
            expression++;
        }
    }
    /* --- set maximum size for numerical arguments --- */
    if (0) {                      /* check turned on or off? */
        if (!isescape && !isdelim) {      /*looking for numerical arg */
            maxsubsz = 96;      /* set max arg size */
        }
    }
/* --- search for matching right} --- */
    for (;;) {                 /*until balanced right} */
        /* --- error check for end-of-string --- */
        if (*expression == '\0') {      /* premature end-of-string */
            if (0 && (!isescape && !isdelim)) { /*looking for numerical arg, */
                expression = origexpression;    /* so end-of-string is error */
                subexpr = origsubexpr;
            }                   /* so reset all ptrs */
            if (isdelim) {      /* generate fake right */
                if (gotescape) {        /* need escaped right */
                    *subexpr++ = '\\';  /* set escape char */
                    *subexpr++ = '.';
                } /* and fake \right. */
                else            /* escape not wanted */
                    *subexpr++ = *rightdelim;
            }                   /* so fake actual right */
            *subexpr = '\0';    /* null-terminate subexpr */
            return (char *) expression;
        }
        /* back with final token */
        /* --- check preceding char for escape --- */
        if (isthischar(*(expression - 1), ESCAPE)) {    /* previous char was \ */
            prevescape = 1 - prevescape;        /* so flip escape flag */
        } else {
            prevescape = 0;     /* or turn flag off */
        }
        /* --- check for { and } (un/escaped as per leading left) --- */
        if (gotescape == prevescape) {  /* escaped iff leading is *//* --- check for (closing) right delim and see if we're done --- */
            if (isthischar(*expression, rightdelim)     /* found a right} */
                ||(isleftdot && isthischar(*expression, right)) /*\left. matches all */
                ||(prevescape && isthischar(*expression, "."))) /*or found \right. */
                if (--nestlevel < 1) {  /*\right balances 1st \left */
                    if (isdelim)        /*caller wants } in subexpr */
                        *subexpr++ = *expression;       /* so end subexpr with } */
                    else /*check for \ before right} */ if (prevescape) /* have unwanted \ */
                        *(subexpr - 1) = '\0';  /* so replace it with null */
                    *subexpr = '\0';    /* null-terminate subexpr */
                    return (char *) (expression + 1);
                }
            /* back with char after } */
            /* --- check for (another) left{ --- */
            if (isthischar(*expression, leftdelim)      /* found another left{ */
                ||(isleftdot && isthischar(*expression, left))) /* any left{ */
                nestlevel++;
        }
        /* --- not done, so copy char to subexpr and continue with next char --- */
        if (++subsz < maxsubsz - 5) {   /* more room in subexpr */
            *subexpr++ = *expression;   /* so copy char and bump ptr */
        }
        expression++;
    }
}


/* ==========================================================================
 * Function:    texleft (expression,subexpr,maxsubsz,ldelim,rdelim)
 * Purpose: scans expression, starting after opening \left,
 *      and returning ptr after matching closing \right.
 *      Everything between is returned in subexpr, if given.
 *      Likewise, if given, ldelim returns delimiter after \left
 *      and rdelim returns delimiter after \right.
 *      If ldelim is given, the returned subexpr doesn't include it.
 *      If rdelim is given, the returned pointer is after that delim.
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char * to first char of null-terminated
 *              string immediately following opening \left
 *      subexpr (O) char * to null-terminated string returning
 *              either everything between balanced
 *              \left ... \right.  If leftdelim given,
 *              subexpr does _not_ contain that delimiter.
 *      maxsubsz (I)    int containing max #bytes returned
 *              in subexpr buffer (0 means unlimited)
 *      ldelim (O)  char * returning delimiter following
 *              opening \left
 *      rdelim (O)  char * returning delimiter following
 *              closing \right
 * --------------------------------------------------------------------------
 * Returns: ( char * )  ptr to the first char of expression
 *              past closing \right, or past closing
 *              right delimiter if rdelim!=NULL,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */

char *texleft(const char *expression, char *subexpr, int maxsubsz, char *ldelim, char *rdelim)
{
    const char *pright = expression;    /* locate matching \right */
    static char left[16] = "\\left", right[16] = "\\right";     /* tex delimiters */
    int sublen = 0;             /* #chars between \left...\right */
/* -------------------------------------------------------------------------
initialization
-------------------------------------------------------------------------- */
/* --- init output --- */
    if (subexpr != NULL) {
        *subexpr = '\0';        /* init subexpr, if given */
    }
    if (ldelim != NULL) {
        *ldelim = '\0';         /* init ldelim,  if given */
    }
    if (rdelim != NULL) {
        *rdelim = '\0';         /* init rdelim,  if given */
    }
/* --- check args --- */
    if (expression == NULL) {
        goto end_of_job;        /* no input supplied */
    }
    if (*expression == '\0') {
        goto end_of_job;        /* nothing after \left */
    }
/* --- determine left delimiter  --- */
    if (ldelim != NULL) {       /* caller wants left delim */
        skipwhite(expression);  /* interpret \left ( as \left( */
        expression = texchar(expression, ldelim);
    }

    /*delim from expression */
    /* -------------------------------------------------------------------------
       locate \right balancing opening \left
       -------------------------------------------------------------------------- */
    /* --- first \right following \left --- */
    if ((pright = strtexchr(expression, right)) != NULL) {      /* look for \right after \left */
        /* --- find matching \right by pushing past any nested \left's --- */
        const char *pleft = expression; /* start after first \left( */
        while (1) {             /*break when matching \right found */
            /* -- locate next nested \left if there is one --- */
            if ((pleft = strtexchr(pleft, left)) == NULL) {     /* find next \left */
                break;          /*no more, so matching \right found */
            }
            pleft += strlen(left);      /* push ptr past \left token */
            if (pleft >= pright) {
                break;          /* not nested if \left after \right */
            }
            /* --- have nested \left, so push forward to next \right --- */
            if ((pright = strtexchr(pright + strlen(right), right)) == NULL) {  /* find next \right */
                break;          /* ran out of \right's */
            }
        }
    }
    /* --- set subexpression length, push pright past \right --- */
    if (pright != (char *) NULL) {      /* found matching \right */
        sublen = (int) (pright - expression);   /* #chars between \left...\right */
        pright += strlen(right);
    }

    /* so push pright past \right */
    /* -------------------------------------------------------------------------
       get rightdelim and subexpr between \left...\right
       -------------------------------------------------------------------------- */
    /* --- get delimiter following \right --- */
    if (rdelim != NULL) {       /* caller wants right delim */
        if (pright == (char *) NULL) {  /* assume \right. at end of exprssn */
            strcpy(rdelim, ".");        /* set default \right. */
            sublen = strlen(expression);        /* use entire remaining expression */
            pright = expression + sublen;
        } /* and push pright to end-of-string */
        else {                  /* have explicit matching \right */
            skipwhite(pright);  /* interpret \right ) as \right) */
            pright = texchar(pright, rdelim);   /* pull delim from expression */
            if (*rdelim == '\0')
                strcpy(rdelim, ".");
        }
    }
    /* or set \right. */
    /* --- get subexpression between \left...\right --- */
    if (sublen > 0)             /* have subexpr */
        if (subexpr != NULL) {  /* and caller wants it */
            if (maxsubsz > 0)
                sublen = min2(sublen, maxsubsz - 1);    /* max buffer size */
            memcpy(subexpr, expression, sublen);        /* stuff between \left...\right */
            subexpr[sublen] = '\0';
        }                       /* null-terminate subexpr */
  end_of_job:
    if (msglevel >= 99 && msgfp != NULL) {
        fprintf(msgfp, "texleft> ldelim=%s, rdelim=%s, subexpr=%.128s\n",
                (ldelim == NULL ? "none" : ldelim),
                (rdelim == NULL ? "none" : rdelim),
                (subexpr == NULL ? "none" : subexpr));
        fflush(msgfp);
    }
    return (char *) pright;
}

/* ==========================================================================
 * Function:    texscripts ( expression, subscript, superscript, which )
 * Purpose: scans expression, returning subscript and/or superscript
 *      if expression is of the form _x^y or ^{x}_{y},
 *      or any (valid LaTeX) permutation of the above,
 *      and a pointer to the first expression char past "scripts"
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char * to first char of null-terminated
 *              string containing valid LaTeX expression
 *              to be scanned
 *      subscript (O)   char * to null-terminated string returning
 *              subscript (without _), if found, or "\0"
 *      superscript (O) char * to null-terminated string returning
 *              superscript (without ^), if found, or "\0"
 *      which (I)   int containing 1 for subscript only,
 *              2 for superscript only, >=3 for either/both
 * --------------------------------------------------------------------------
 * Returns: ( char * )  ptr to the first char of expression
 *              past returned "scripts" (unchanged
 *              except for skipped whitespace if
 *              neither subscript nor superscript found),
 *              or NULL for any parsing error.
 * --------------------------------------------------------------------------
 * Notes:     o an input expression like ^a^b_c will return superscript="b",
 *      i.e., totally ignoring all but the last "script" encountered
 * ======================================================================= */

const char *texscripts(const char *expression, char *subscript, char *superscript, int which)
{
    int gotsub = 0, gotsup = 0; /* check that we don't eat, e.g., x_1_2 */

    if (subscript != NULL) {
        *subscript = '\0';      /*init in case no subscript */
    }
    if (superscript != NULL) {
        *superscript = '\0';    /*init in case no super */
    }
/* -------------------------------------------------------------------------
get subscript and/or superscript from expression
-------------------------------------------------------------------------- */
    while (expression != NULL) {
        skipwhite(expression);  /* leading whitespace gone */
        if (*expression == '\0') {
            return expression;        /* nothing left to scan */
        }
        if (isthischar(*expression, SUBSCRIPT)  /* found _ */
            &&(which == 1 || which > 2)) {      /* and caller wants it */
            if (gotsub || subscript == NULL) {
                break;          /* or no subscript buffer */
            }
            gotsub = 1;         /* set subscript flag */
            expression = texsubexpr(expression + 1, subscript, 0, "{", "}", 0, 0);
        } else if (isthischar(*expression, SUPERSCRIPT) && which >= 2) {
            /* found ^ and caller wants it */
            if (gotsup  || superscript == NULL) {
                break;          /* found 2nd superscript or no superscript buffer */
            }
            gotsup = 1;
            expression = texsubexpr(expression + 1, superscript, 0, "{", "}", 0, 0);
        } else {                 /* neither _ nor ^ */
            return expression;
        }
    }
    return expression;
}

/* ==========================================================================
 * Function:    isbrace ( expression, braces, isescape )
 * Purpose: checks leading char(s) of expression for a brace,
 *      either escaped or unescaped depending on isescape,
 *      except that { and } are always matched, if they're
 *      in braces, regardless of isescape.
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char * to first char of null-terminated
 *              string containing a valid LaTeX expression
 *              whose leading char(s) are checked for braces
 *              that begin subexpression, e.g., "{[(<"
 *      braces (I)  char * specifying matching brace delimiters
 *              to be checked for, e.g., "{[(<" or "}])>"
 *      isescape (I)    int containing 0 to match only unescaped
 *              braces, e.g., (...) or {...}, etc,
 *              or containing 1 to match only escaped
 *              braces, e.g., \(...\) or \[...\], etc,
 *              or containing 2 to match either.
 *              But note: if {,} are in braces
 *              then they're *always* matched whether
 *              escaped or not, regardless of isescape.
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if the leading char(s) of expression
 *              is a brace, or 0 if not.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */

static int isbrace(const char *expression, const char *braces, int isescape)
{
    int gotescape = 0;          /* true if leading char is an escape */
    int gotbrace = 0;           /*true if first non-escape char is a brace */

/* --- first check for end-of-string or \= ligature --- */
    if (*expression == '\0' || isligature) {
        goto end_of_job;        /* have a \= ligature */
    }
/* --- check leading char for escape --- */
    if (isthischar(*expression, ESCAPE)) {      /* expression is escaped */
        gotescape = 1;          /* so set flag accordingly */
        expression++;
    }
    /* and bump past escape */
    /* --- check (maybe next char) for brace --- */
    if (isthischar(*expression, braces)) {
        gotbrace = 1;          /* so set flag accordingly */
    }
    if (gotescape && *expression == '.') {        /* \. matches any brace */
        gotbrace = 1;
    }

    if (gotbrace && isthischar(*expression, "{}")) {      /*expression has TeX brace */
        if (isescape) {
            isescape = 2;       /* reset escape flag */
        }
    }

  end_of_job:
    if (msglevel >= 999 && msgfp != NULL) {
        fprintf(msgfp,
                "isbrace> expression=%.8s, gotbrace=%d (isligature=%d)\n",
                expression, gotbrace, isligature);
        fflush(msgfp);
    }
    if (gotbrace && (isescape == 2 || gotescape == isescape)) {
        return 1;
    }
    return 0;
}

/* ==========================================================================
 * Function:    preamble ( expression, size, subexpr )
 * Purpose: parses $-terminated preamble, if present, at beginning
 *      of expression, re-setting size if necessary, and
 *      returning any other parameters besides size in subexpr.
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char * to first char of null-terminated
 *              string containing LaTeX expression possibly
 *              preceded by $-terminated preamble
 *      size (I/O)  int *  containing 0-4 default font size,
 *              and returning size modified by first
 *              preamble parameter (or unchanged)
 *      subexpr(O)  char *  returning any remaining preamble
 *              parameters past size
 * --------------------------------------------------------------------------
 * Returns: ( char * )  ptr to first char past preamble in expression
 *              or NULL for any parsing error.
 * --------------------------------------------------------------------------
 * Notes:     o size can be any number >=0. If preceded by + or -, it's
 *      interpreted as an increment to input size; otherwise
 *      it's interpreted as the size.
 *        o if subexpr is passed as NULL ptr, then returned expression
 *      ptr will have "flushed" and preamble parameters after size
 * ======================================================================= */

char *preamble(char *expression, int *size, char *subexpr)
{
    char pretext[512];
    char *prep = expression;    /*pream from expression, ptr into it */
    char *dollar;
    char *comma;                /* preamble delimiters */
    int prelen = 0;             /* preamble length */
    int sizevalue = 0;          /* value of size parameter */
    int isfontsize = 0;         /*true if leading fontsize present */
    int isdelta = 0;            /*true to increment passed size arg */

    if (subexpr != NULL) {        /* caller passed us an address */
        *subexpr = '\0';        /* so init assuming no preamble */
    }
    if (expression == NULL) {
        goto end_of_job;        /* no input */
    }
    if (*expression == '\0') {
        goto end_of_job;        /* input is an empty string */
    }

/* -------------------------------------------------------------------------
process preamble if present
-------------------------------------------------------------------------- */
    if ((dollar = strchr(expression, '$')) != NULL) {
        if ((prelen = (int) (dollar - expression)) > 0) {
            /* #chars in expression preceding $ must have preamble preceding $ */
            if (prelen < 65) {  /* too long for a prefix */
                memcpy(pretext, expression, prelen);    /* local copy of preamble */
                pretext[prelen] = '\0'; /* null-terminated */
                if (strchr(pretext, *(ESCAPE)) == NULL  /*shouldn't be an escape in preamble */
                    && strchr(pretext, '{') == NULL) {  /*shouldn't be a left{ in preamble */
                    /* --- skip any leading whitespace  --- */
                    prep = pretext;     /* start at beginning of preamble */
                    skipwhite(prep);    /* skip any leading white space */
                    /* --- check for embedded , or leading +/- (either signalling size) --- */
                    if (isthischar(*prep, "+-")) {        /* have leading + or - */
                        isdelta = 1;    /* so use size value as increment */
                    }
                    comma = strchr(pretext, ',');       /* , signals leading size param */
                    /* --- process leading size parameter if present --- */
                    if (comma != NULL   /* size param explicitly signalled */
                        || isdelta || isdigit(*prep)) { /* or inferred implicitly */
                        /* --- parse size parameter and reset size accordingly --- */
                        if (comma != NULL)
                            *comma = '\0';      /*, becomes null, terminating size */
                        sizevalue = atoi(prep); /* convert size string to integer */
                        if (size != NULL)       /* caller passed address for size */
                            *size = (isdelta ? *size + sizevalue : sizevalue);  /* so reset size */
                        /* --- finally, set flag and shift size parameter out of preamble --- */
                        isfontsize = 1; /*set flag showing font size present */
                        if (comma != NULL) {    /*2/15/12-isn't this superfluous??? */
                            strsqueezep(pretext, comma + 1);
                    } /* squeeze out leading size param */ }
                    /* --- end-of-if(comma!=NULL||etc) --- *//* --- copy any preamble params following size to caller's subexpr --- */
                    if (comma != NULL || !isfontsize) {
                        /*preamb contains params past size */
                        if (subexpr != NULL) {    /* caller passed us an address */
                             strcpy(subexpr, pretext);  /*so return extra params to caller */
                        }
                    }
                    /* --- finally, set prep to shift preamble out of expression --- */
                    prep = expression + prelen + 1;     /* set prep past $ in expression */
                }
            }
        } else {                  /* $ is first char of expression */
            int ndollars = 0;   /* number of $...$ pairs removed */
            prep = expression;  /* start at beginning of expression */
            while (*prep == '$') {      /* remove all matching $...$'s */
                int explen = strlen(prep) - 1;  /* index of last char in expression */
                if (explen < 2) {
                    break;      /* no $...$'s left to remove */
                }
                if (prep[explen] != '$') {
                    break;      /* unmatched $ */
                }
                prep[explen] = '\0';    /* remove trailing $ */
                prep++;         /* and remove matching leading $ */
                ndollars++;     /* count another pair removed */
            }                   /* --- end-of-while(*prep=='$') --- */
            ispreambledollars = ndollars;       /* set flag to fix \displaystyle */
            if (ndollars == 1) {  /* user submitted $...$ expression */
                isdisplaystyle = 0;     /* so set \textstyle */
            }
            if (ndollars > 1) {  /* user submitted $$...$$ */
                isdisplaystyle = 2;     /* so set \displaystyle */
            }
            /*goto process_preamble; */
            /*check for preamble after leading $ */
        }
    }
  end_of_job:
    return prep;              /*expression, or ptr past preamble */
}

/* ==========================================================================
 * Function:    mimeprep ( expression )
 * Purpose: preprocessor for mimeTeX input, e.g.,
 *      (a) removes comments,
 *      (b) converts \left( to \( and \right) to \),
 *      (c) xlates &html; special chars to equivalent latex
 *      Should only be called once (after unescape_url())
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char * to first char of null-terminated
 *              string containing mimeTeX/LaTeX expression,
 *              and returning preprocessed string
 * --------------------------------------------------------------------------
 * Returns: ( char * )  ptr to input expression,
 *              or NULL for any parsing error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
const char *mimeprep(const char *expression)
{
    const char *expptr = expression;    /* ptr within expression */
    char *tokptr = NULL;        /*ptr to token found in expression */
    char argval[8192];          /*parse for macro args after token */
    int idelim = 0;             /* left- or right-index */
    int isymbol = 0;            /*symbols[],rightcomment[],etc index */
    int xlateleft = 0;          /* true to xlate \left and \right */
    char *leftptr = NULL;       /* find leftcomment in expression */
    static const char *leftcomment = "%%";      /* open comment */
    static const char *rightcomment[] = {
        "\n", "%%", NULL
    };

    static const char *leftfrom[] =     /* xlate any \left suffix... */
    {
        "\\|",              /* \left\| */
        "\\{",              /* \left\{ */
        "\\langle",         /* \left\langle */
        NULL
    };
    static const char *leftto[] =       /* ...to this instead */
    {
        "=",                /* = */
        "{",                /* { */
        "<",                /* < */
        NULL
    };
    static const char *rightfrom[] =    /* xlate any \right suffix... */
    {
        "\\|",                  /* \right\| */
        "\\}",              /* \right\} */
        "\\rangle",         /* \right\rangle */
        NULL
    };
    static const char *rightto[] =      /* ...to this instead */
    {
        "=",                /* = */
        "}",                /* } */
        ">",                /* > */
        NULL
    };
/* ---
 * { \atop }-like commands
 * ----------------------- */
    const char *atopsym = NULL; /* atopcommands[isymbol] */
    static const char *atopcommands[] = /* list of {a+b\command c+d}'s */
    {
        "\\over",           /* plain tex for \frac */
        "\\choose",         /* binomial coefficient */
#ifndef NOATOP              /*noatop preserves old mimeTeX rule */
        "\\atop",
#endif
    NULL};                      /* --- end-of-atopcommands[] --- */
    static const char *atopdelims[] =   /* delims for atopcommands[] */
    {
        NULL, NULL,             /* \\over has no delims */
        "\\left(", "\\right)",      /* \\choose has ( ) delims */
#ifndef NOATOP                  /*noatop preserves old mimeTeX rule */
        NULL, NULL,         /* \\atop has no delims */
#endif
        NULL, NULL
    };
/* ---
 * html special/escape chars converted to latex equivalents
 * -------------------------------------------------------- */
    const char *htmlsym = NULL; /* symbols[isymbol].html */
    static struct {
        const char *html;
        const char *args;
        const char *latex;
    } symbols[] = {
        /* --------------------------------------------
           Specials        termchar  value...
           -------------------------------------------- */
        {"\\version", NULL, "{\\small\\text \\fbox{\\begin{gather}"
         "mime\\TeX version \\versionnumber \\\\"
         "last revised \\revisiondate \\\\ \\copyrighttext \\\\"
         "see \\homepagetext for details \\end{gather}}}"},
        {"\\copyright", NULL,
         "{\\small\\red\\text \\fbox{\\begin{gather}"
         "mimeTeX \\copyrighttext \\\\"
         "see \\homepagetext for details \\end{gather}}}"},
        {"\\versionnumber", NULL, "{\\text " VERSION "}"},
        {"\\revisiondate", NULL, "{\\text " REVISIONDATE "}"},
        {"\\copyrighttext", NULL, "{\\text " COPYRIGHTTEXT ".}"},
        {"\\homepagetext", NULL,
         "{\\text http://www.forkosh.com/mimetex.html}"},
        /* --------------------------------------------
           Cyrillic  termchar  mimeTeX equivalent...
           -------------------------------------------- */
        {"\\\'G", "embed\\", "{\\acute{G}}"},
        {"\\\'g", "embed\\", "{\\acute{g}}"},
        {"\\\'K", "embed\\", "{\\acute{K}}"},
        {"\\\'k", "embed\\", "{\\acute{k}}"},
        {"\\u U", "embed\\", "{\\breve{U}}"},
        {"\\u u", "embed\\", "{\\breve{u}}"},
        /*{ "\\\"E", "embed\\","{\\ddot{E}}" }, */
        /*{ "\\\"e", "embed\\","{\\ddot{e}}" }, */
        {"\\\"I", "embed\\", "{\\ddot{\\=I}}"},
        {"\\\"\\i", "embed\\", "{\\ddot{\\=\\i}}"},
        /* --------------------------------------------
           LaTeX Macro #args,default  template...
           -------------------------------------------- */
        {"\\lvec", "2n", "{#2_1,\\cdots,#2_{#1}}"},
        {"\\grave", "1", "{\\stackrel{\\Huge\\gravesym}{#1}}"},  /* \grave */
        {"\\acute", "1", "{\\stackrel{\\Huge\\acutesym}{#1}}"},  /* \acute */
        {"\\check", "1", "{\\stackrel{\\Huge\\checksym}{#1}}"},  /* \check */
        {"\\breve", "1", "{\\stackrel{\\Huge\\brevesym}{#1}}"},  /* \breve */
        {"\\buildrel", "3", "{\\stackrel{#1}{#3}}"},     /* ignore #2 = \over */
        {"\\overset", NULL, "\\stackrel"},       /* just an alias */
        {"\\underset", "2", "\\relstack{#2}{#1}"},       /* reverse args */
        {"\\dfrac", "2", "{\\frac{#1}{#2}}"},
        {"\\binom", "2", "{\\begin{pmatrix}{#1}\\\\{#2}\\end{pmatrix}}"},
        {"\\aangle", "26", "{\\boxaccent{#1}{#2}}"},
        {"\\actuarial", "2 ", "{#1\\boxaccent{6}{#2}}"}, /*comprehensive sym list */
        {"\\boxaccent", "2", "{\\fbox[,#1]{#2}}"},

#if 0
        /* --------------------------------------------
           html char termchar  LaTeX equivalent...
           -------------------------------------------- */
        {"&quot", ";", "\""},    /* &quot; is first, &#034; */
        {"&amp", ";", "&"},
        {"&lt", ";", "<"},
        {"&gt", ";", ">"},
        /*{ "&#092", ";",    "\\" }, *//* backslash */
        {"&backslash", ";", "\\"},
        {"&nbsp", ";", "~"},
        {"&iexcl", ";", "{\\raisebox{-2}{\\rotatebox{180}{!}}}"},
        {"&brvbar", ";", "|"},
        {"&plusmn", ";", "\\pm"},
        {"&sup2", ";", "{{}^2}"},
        {"&sup3", ";", "{{}^3}"},
        {"&micro", ";", "\\mu"},
        {"&sup1", ";", "{{}^1}"},
        {"&frac14", ";", "{\\frac14}"},
        {"&frac12", ";", "{\\frac12}"},
        {"&frac34", ";", "{\\frac34}"},
        {"&iquest", ";", "{\\raisebox{-2}{\\rotatebox{180}{?}}}"},
        {"&Acirc", ";", "{\\rm~\\hat~A}"},
        {"&Atilde", ";", "{\\rm~\\tilde~A}"},
        {"&Auml", ";", "{\\rm~\\ddot~A}"},
        {"&Aring", ";", "{\\rm~A\\limits^{-1$o}}"},
        {"&atilde", ";", "{\\rm~\\tilde~a}"},
        {"&yuml", ";", "{\\rm~\\ddot~y}"},       /* &yuml; is last, &#255; */
        {"&#", ";", "{[\\&\\#nnn?]}"},   /* all other explicit &#nnn's */
        /* --------------------------------------------
           html tag     termchar    LaTeX equivalent...
           -------------------------------------------- */
        {"< br >", "embed\\i", "\\\\"},
        {"< br / >", "embed\\i", "\\\\"},
        {"< dd >", "embed\\i", " \000"},
        {"< / dd >", "embed\\i", " \000"},
        {"< dl >", "embed\\i", " \000"},
        {"< / dl >", "embed\\i", " \000"},
        {"< p >", "embed\\i", " \000"},
        {"< / p >", "embed\\i", " \000"},
        /* --------------------------------------------
           garbage      termchar  LaTeX equivalent...
           -------------------------------------------- */
        {"< tex >", "embed\\i", " \000"},
        {"< / tex >", "embed\\i", " \000"},
#endif
        /* --------------------------------------------
           LaTeX   termchar   mimeTeX equivalent...
           -------------------------------------------- */
        {"\\AA", NULL, "{\\rm~A\\limits^{-1$o}}"},
        {"\\aa", NULL, "{\\rm~a\\limits^{-1$o}}"},
        {"\\bmod", NULL, "{\\hspace2{\\rm~mod}\\hspace2}"},
        {"\\vdots", NULL, "{\\raisebox3{\\rotatebox{90}{\\ldots}}}"},
        {"\\dots", NULL, "{\\cdots}"},
        {"\\cdots", NULL, "{\\raisebox3{\\ldots}}"},
        {"\\ldots", NULL, "{\\fs4.\\hspace1.\\hspace1.}"},
        {"\\ddots", NULL, "{\\fs4\\raisebox8.\\hspace1\\raisebox4.\\hspace1\\raisebox0.}"},
        {"\\notin", NULL, "{\\not\\in}"},
        {"\\neq", NULL, "{\\not=}"},
        {"\\ne", NULL, "{\\not=}"},
        {"\\mapsto", NULL, "{\\rule[fs/2]{1}{5+fs}\\hspace{-99}\\to}"},
        {"\\hbar", NULL, "{\\compose~h{{\\fs{-1}-\\atop\\vspace3}}}"},
        {"\\angle", NULL, "{\\compose{\\hspace{3}\\lt}{\\circle(10,15;-80,80)}}"},
        {"\\textcelsius", NULL, "{\\textdegree C}"},
        {"\\textdegree", NULL, "{\\Large^{^{\\tiny\\mathbf o}}}"},
        {"\\cr", NULL, "\\\\"},
        /*{ "\\colon",   NULL,   "{:}" }, */
        {"\\iiint", NULL, "{\\int\\int\\int}\\limits"},
        {"\\iint", NULL, "{\\int\\int}\\limits"},
        {"\\Bigiint", NULL, "{\\Bigint\\Bigint}\\limits"},
        {"\\bigsqcap", NULL, "{\\fs{+4}\\sqcap}"},
        {"\\_", "embed", "{\\underline{\\ }}"},  /* displayed underscore */
        {"!`", NULL, "{\\raisebox{-2}{\\rotatebox{180}{!}}}"},
        {"?`", NULL, "{\\raisebox{-2}{\\rotatebox{180}{?}}}"},
        {"^\'", "embed", "\'"},  /* avoid ^^ when re-xlating \' below */
        {"\'\'\'\'", "embed", "^{\\fs{-1}\\prime\\prime\\prime\\prime}"},
        {"\'\'\'", "embed", "^{\\fs{-1}\\prime\\prime\\prime}"},
        {"\'\'", "embed", "^{\\fs{-1}\\prime\\prime}"},
        {"\'", "embed", "^{\\fs{-1}\\prime}"},
        {"\\rightleftharpoons", NULL, "{\\rightharpoonup\\atop\\leftharpoondown}"},
        {"\\therefore", NULL, "{\\Huge\\raisebox{-4}{.\\atop.\\,.}}"},
        {"\\LaTeX", NULL, "{\\rm~L\\raisebox{3}{\\fs{-1}A}\\TeX}"},
        {"\\TeX", NULL, "{\\rm~T\\raisebox{-3}{E}X}"},
        {"\\cyan", NULL, "{\\reverse\\red\\reversebg}"},
        {"\\magenta", NULL, "{\\reverse\\green\\reversebg}"},
        {"\\yellow", NULL, "{\\reverse\\blue\\reversebg}"},
        {"\\cancel", NULL, "\\Not"},
        {"\\hhline", NULL, "\\Hline"},
        {"\\Hline", NULL, "\\hline\\,\\\\\\hline"},
        /* -----------------------------------------------------------------------
           As per emails with Zbigniew Fiedorowicz <fiedorow@math.ohio-state.edu>
           "Algebra Syntax"  termchar   mimeTeX/LaTeX equivalent...
           ----------------------------------------------------------------------- */
        {"sqrt", "1", "{\\sqrt{#1}}"},
        {"sin", "1", "{\\sin{#1}}"},
        {"cos", "1", "{\\cos{#1}}"},
        {"asin", "1", "{\\sin^{-1}{#1}}"},
        {"acos", "1", "{\\cos^{-1}{#1}}"},
        {"exp", "1", "{{\\rm~e}^{#1}}"},
        {"det", "1", "{\\left|{#1}\\right|}"},
        /* --------------------------------------------
           LaTeX Constant    termchar   value...
           -------------------------------------------- */
        {"\\thinspace", NULL, "\\,"},
        {"\\thinmathspace", NULL, "\\,"},
        {"\\textwidth", NULL, "400"},
        /* --- end-of-table indicator --- */
        {NULL, NULL, NULL}
    };
/* ---
 * html &#nn chars converted to latex equivalents
 * ---------------------------------------------- */
    int htmlnum = 0;            /* numbers[inum].html */
    static struct {
        int html;
        const char *latex;
    } numbers[] = {
     /* ---------------------------------------
       html num  LaTeX equivalent...
       --------------------------------------- */
        {  9, " "},              /* horizontal tab */
        { 10, " "},              /* line feed */
        { 13, " "},              /* carriage return */
        { 32, " "},              /* space */
        { 33, "!"},              /* exclamation point */
        { 34, "\""},             /* &quot; */
        { 35, "#"},              /* hash mark */
        { 36, "$"},              /* dollar */
        { 37, "%"},              /* percent */
        { 38, "&"},              /* &amp; */
        { 39, "\'"},             /* apostrophe (single quote) */
        { 40, ")"},              /* left parenthesis */
        { 41, ")"},              /* right parenthesis */
        { 42, "*"},              /* asterisk */
        { 43, "+"},              /* plus */
        { 44, ","},              /* comma */
        { 45, "-"},              /* hyphen (minus) */
        { 46, "."},              /* period */
        { 47, "/"},              /* slash */
        { 58, ":"},              /* colon */
        { 59, ";"},              /* semicolon */
        { 60, "<"},              /* &lt; */
        { 61, "="},              /* = */
        { 62, ">"},              /* &gt; */
        { 63, "\?"},             /* question mark */
        { 64, "@"},              /* commercial at sign */
        { 91, "["},              /* left square bracket */
        { 92, "\\"},             /* backslash */
        { 93, "]"},              /* right square bracket */
        { 94, "^"},              /* caret */
        { 95, "_"},              /* underscore */
        { 96, "`"},              /* grave accent */
        {123, "{"},              /* left curly brace */
        {124, "|"},              /* vertical bar */
        {125, "}"},              /* right curly brace */
        {126, "~"},              /* tilde */
        {160, "~"},              /* &nbsp; (use tilde for latex) */
        {166, "|"},              /* &brvbar; (broken vertical bar) */
        {173, "-"},              /* &shy; (soft hyphen) */
        {177, "{\\pm}"},         /* &plusmn; (plus or minus) */
        {215, "{\\times}"},      /* &times; (plus or minus) */
        {-999, NULL}
    };

/* -------------------------------------------------------------------------
first remove comments
-------------------------------------------------------------------------- */
    expptr = expression;        /* start search at beginning */
    while ((leftptr = strstr(expptr, leftcomment)) != NULL) {   /*found leftcomment */
        const char *rightsym = NULL;    /* rightcomment[isymbol] */
        expptr = leftptr + strlen(leftcomment); /* start rightcomment search here */
        /* --- check for any closing rightcomment, in given precedent order --- */
        if (*expptr != '\0') {  /*have chars after this leftcomment */
            for (isymbol = 0; (rightsym = rightcomment[isymbol]) != NULL; isymbol++) {
                if ((tokptr = strstr(expptr, rightsym)) != NULL) {      /*found rightcomment */
                    tokptr += strlen(rightsym); /* first char after rightcomment */
                    if (*tokptr == '\0') {      /*nothing after this rightcomment */
                        *leftptr = '\0';        /*so terminate expr at leftcomment */
                        break;
                    }           /* and stop looking for comments */
                    *leftptr = '~';     /* replace entire comment by ~ */
                    strsqueezep(leftptr + 1, tokptr);   /* squeeze out comment */
                    goto next_comment;
                }
            }
        }
        /* stop looking for rightcomment */
        /* --- no rightcomment after opening leftcomment --- */
        *leftptr = '\0';        /* so terminate expression */
        /* --- resume search past squeezed-out comment --- */
      next_comment:
        if (*leftptr == '\0') {
            break;              /* reached end of expression */
        }
        expptr = leftptr + 1;   /*resume search after this comment */
    }
/* -------------------------------------------------------------------------
run thru table, converting all occurrences of each macro to its expansion
-------------------------------------------------------------------------- */
    for (isymbol = 0; (htmlsym = symbols[isymbol].html) != NULL; isymbol++) {
        int htmllen = strlen(htmlsym);  /* length of escape, _without_ ; */
        int isalgebra = isalpha((int) (*htmlsym));      /* leading char alphabetic */
        int isembedded = 0;     /* true to xlate even if embedded */
        int istag = 0 /* not used --, isamp = 0 */ ;    /* true for <tag>, &char; symbols */
        int isstrwstr = 0;      /* true to use strwstr() */
        int wstrlen = 0;        /* length of strwstr() match */
        const char *aleft = "{([<|";
        const char *aright = "})]>|";   /*left,right delims for alg syntax */
        char embedkeywd[99] = "embed";  /* keyword to signal embedded token */
        char embedterm = '\0';  /* char immediately after embed */
        int embedlen = strlen(embedkeywd);      /* #chars in embedkeywd */
        const char *args = symbols[isymbol].args;       /* number {}-args, optional []-arg */
        const char *htmlterm = args;    /*if *args nonumeric, then html term */
        const char *latexsym = symbols[isymbol].latex;  /*latex replacement for htmlsym */
        char errorsym[256];     /*or latexsym may point to error msg */
        char abuff[8192];
        int iarg, nargs = 0;    /* macro expansion params */
        char wstrwhite[99];     /* whitespace chars for strwstr() */
        skipwhite(htmlsym);     /*skip any bogus leading whitespace */
        htmllen = strlen(htmlsym);      /* reset length of html token */
        istag = (isthischar(*htmlsym, "<") ? 1 : 0);    /* html <tag> starts with < */

        /* not used -- isamp = (isthischar(*htmlsym, "&") ? 1 : 0); *//* html &char; starts with & */
        if (args != NULL) {     /*we have args (or htmlterm) param */
            if (*args != '\0') {        /* and it's not an empty string */
                if (strchr("0123456789", *args) != NULL) {      /* is 1st char #args=0-9 ? */
                    htmlterm = NULL;    /* if so, then we have no htmlterm */
                    *abuff = *args;
                    abuff[1] = '\0';    /* #args char in ascii buffer */
                    nargs = atoi(abuff);
                } else if (strncmp(args, embedkeywd, embedlen) == 0) {    /*xlate embedded token */
                    int arglen = strlen(args);  /* length of "embed..." string */
                    htmlterm = NULL;    /* if so, then we have no htmlterm */
                    isembedded = 1;     /* turn on embedded flag */
                    if (arglen > embedlen)      /* have embed "allow escape" flag */
                        embedterm = args[embedlen];     /* char immediately after "embed" */
                    if (arglen > embedlen + 1) {        /* have embed,flag,white for strwstr */
                        isstrwstr = 1;  /* turn on strwtsr flag */
                        strcpy(wstrwhite, args + embedlen + 1);
                    }
                }               /*and set its whitespace arg */
            }
        }
        expptr = expression;    /* re-start search at beginning */
        while ((tokptr = (!isstrwstr ? strstr(expptr, htmlsym) : strwstr(expptr, htmlsym, wstrwhite, &wstrlen))) != NULL) {
            /* just use strtsr or use our strwstr */
            /* found another sym */
            int toklen = (!isstrwstr ? htmllen : wstrlen);      /* length of matched sym */
            char termchar = *(tokptr + toklen), /* char terminating html sequence */
                prevchar = (tokptr == expptr ? ' ' : *(tokptr - 1));    /*char preceding html */
            int isescaped = (isthischar(prevchar, ESCAPE) ? 1 : 0);     /* token escaped? */
            int escapelen = toklen;     /* total length of escape sequence */
            int isflush = 0;    /* true to flush (don't xlate) */
            /* --- check odd/even backslashes preceding tokens --- */
            if (isescaped) {    /* have one preceding backslash */
                const char *p = tokptr - 1;     /* ptr to that preceding backslash */
                while (p != expptr) {   /* and we may have more preceding */
                    p--;
                    if (!isthischar(*p, ESCAPE)) {
                        break;  /* but we don't, so quit */
                    }
                    isescaped = 1 - isescaped;
                }
            }
            /* or flip isescaped flag if we do */
            /* --- init with "trivial" abuff,escapelen from symbols[] table --- */
            *abuff = '\0';      /* default to empty string */
            if (latexsym != NULL) {      /* table has .latex xlation */
                if (*latexsym != '\0') { /* and it's not an empty string */
                    strcpy(abuff, latexsym);    /* so get local copy */
                }
            }
            if (!isembedded) {   /*embedded sequences not terminated */
                if (htmlterm != NULL) {  /* sequence may have terminator */
                    escapelen += (isthischar(termchar, htmlterm) ? 1 : 0);      /*add terminator */
                }
            }
            /* --- don't xlate if we just found prefix of longer symbol, etc --- */
            if (!isembedded) {  /* not embedded */
                if (isescaped) { /* escaped */
                    isflush = 1;        /* set flag to flush escaped token */
                }
                if (!istag && isalpha((int) termchar)) { /* followed by alpha */
                    isflush = 1;        /* so just a prefix of longer symbol */
                }
                if (isalpha((int) (*htmlsym))) { /* symbol starts with alpha */
                    if ((!isspace(prevchar) && isalpha(prevchar))) {     /* just a suffix */
                        isflush = 1;
                    }
                }
            }                   /* set flag to flush token */
            if (isembedded) {    /* for embedded token */
                if (isescaped) {  /* and embedded \token escaped */
                    if (!isthischar(embedterm, ESCAPE)) { /* don't xlate escaped \token */
                        isflush = 1;
                    }
                }
            }
            if (isflush) {      /* don't xlate this token */
                expptr = tokptr + 1;    /*toklen; *//* just resume search after token */
                continue;
            }
            /* but don't replace it */
            /* --- check for &# prefix signalling &#nnn; --- */
            if (strcmp(htmlsym, "&#") == 0) {   /* replacing special &#nnn; chars */
                /* --- accumulate chars comprising number following &# --- */
                char anum[32];  /* chars comprising number after &# */
                int inum = 0;   /* no chars accumulated yet */
                while (termchar != '\0') {      /* don't go past end-of-string */
                    if (!isdigit((int) termchar)) {
                        break;  /* and don't go past digits */
                    }
                    if (inum > 10) {
                        break;  /* some syntax error in expression */
                    }
                    anum[inum] = termchar;      /* accumulate this digit */
                    inum++;
                    toklen++;   /* bump field length, token length */
                    termchar = *(tokptr + toklen);
                }
                anum[inum] = '\0';      /* null-terminate anum */
                escapelen = toklen;     /* length of &#nnn; sequence */
                if (htmlterm != NULL)   /* sequence may have terminator */
                    escapelen += (isthischar(termchar, htmlterm) ? 1 : 0);      /*add terminator */
                /* --- look up &#nnn in number[] table --- */
                htmlnum = atoi(anum);   /* convert anum[] to an integer */
                strninit(errorsym, latexsym, 128);      /* init error message */
                latexsym = errorsym;    /* init latexsym as error message */

                // XXX: 1st argument is not read only
                strreplace((char *) latexsym, "nnn", anum, 1);  /*place actual &#num in message */

                for (inum = 0; numbers[inum].html >= 0; inum++) {       /* run thru numbers[] */
                    if (htmlnum == numbers[inum].html) {        /* till we find a match */
                        latexsym = numbers[inum].latex; /* latex replacement */
                        break;
                    }           /* no need to look any further */
                }
                if (latexsym != NULL) { /* table has .latex xlation */
                    if (*latexsym != '\0') {    /* and it's not an empty string */
                        strcpy(abuff, latexsym);        /* so get local copy */
                    }
                }
            }
            /* --- substitute macro arguments --- */
            if (nargs > 0) {    /*substitute #1,#2,... in latexsym */
                const char *arg1ptr = tokptr + escapelen;       /* nargs begin after macro literal */
                const char *optarg = args + 1;  /* ptr 1 char past #args digit 0-9 */
                expptr = arg1ptr;       /* ptr to beginning of next arg */
                for (iarg = 1; iarg <= nargs; iarg++) { /* one #`iarg` arg at a time */
                    char argsignal[32] = "#1";  /* #1...#9 signals arg replacement */
                    char *argsigptr = NULL;     /* ptr to argsignal in abuff[] */
                    /* --- get argument value --- */
                    *argval = '\0';     /* init arg as empty string */
                    skipwhite(expptr);  /* and skip leading white space */
                    if (iarg == 1 && *optarg != '\0'    /* check for optional [arg] */
                        && !isalgebra) {        /* but not in "algebra syntax" */
                        strcpy(argval, optarg); /* init with default value */
                        if (*expptr == '[') {   /* but user gave us [argval] */
                            expptr = texsubexpr(expptr, argval, 0, "[", "]", 0, 0);
                        }
                    } else if (*expptr != '\0') {
                        /* check that some argval provided */
                        /* not optional, so get {argval} */
                        if (!isalgebra) {       /* only { } delims for latex macro */
                            expptr = texsubexpr(expptr, argval, 0, "{", "}", 0, 0);     /*get {argval} */
                        } else {        /*any delim for algebra syntax macro */
                            expptr =
                                texsubexpr(expptr, argval, 0, aleft,
                                           aright, 0, 1);
                            if (isthischar(*argval, aleft)) {   /* have delim-enclosed arg */
                                if (*argval != '{') {   /* and it's not { }-enclosed */
                                    strchange(0, argval, "\\left");     /* insert opening \left, */
                                    strchange(0,
                                              argval + strlen(argval) - 1,
                                              "\\right");
                                }
                            }
                        }       /*\right */
                    }
                    /* --- (recursively) call mimeprep() to prep the argument --- */
                    if (!isempty(argval)) {      /* have an argument */
                        mimeprep(argval);       /* so (recursively) prep it */
                    }
                    /* --- replace #`iarg` in macro with argval --- */
                    sprintf(argsignal, "#%d", iarg);    /* #1...#9 signals argument */
                    while ((argsigptr = strstr(argval, argsignal)) != NULL) {   /* #1...#9 */
                        strsqueeze(argsigptr, strlen(argsignal));
                    }           /* can't be in argval */
                    while ((argsigptr = strstr(abuff, argsignal)) != NULL)      /* #1...#9 */
                        strchange(strlen(argsignal), argsigptr, argval);        /*replaced by argval */
                }               /* --- end-of-for(iarg) --- */
                escapelen += ((int) (expptr - arg1ptr));        /* add in length of all args */
            }                   /* --- end-of-if(nargs>0) --- */
            strchange(escapelen, tokptr, abuff);        /*replace macro or html symbol */
            expptr = tokptr + strlen(abuff);    /*resume search after macro / html */
        }                       /* --- end-of-while(tokptr!=NULL) --- */
    }                           /* --- end-of-for(isymbol) --- */
/* -------------------------------------------------------------------------
convert \left( to \(  and  \right) to \),  etc.
-------------------------------------------------------------------------- */
    if (xlateleft) {             /* \left...\right xlation wanted */
        for (idelim = 0; idelim < 2; idelim++) {        /* 0 for \left  and  1 for \right */
            const char *lrstr = (idelim == 0 ? "\\left" : "\\right");   /* \left on 1st pass */
            int lrlen = (idelim == 0 ? 5 : 6);  /* strlen() of \left or \right */
            const char *braces = (idelim == 0 ? LEFTBRACES "." : RIGHTBRACES "."),      /*([{<or)]}> */
                **lrfrom = (idelim == 0 ? leftfrom : rightfrom),        /* long braces like \| */
                **lrto = (idelim == 0 ? leftto : rightto),      /* xlated to 1-char like = */
                *lrsym = NULL;  /* lrfrom[isymbol] */
            expptr = expression;        /* start search at beginning */
            while ((tokptr = strstr(expptr, lrstr)) != NULL) {  /* found \left or \right */
                if (isthischar(*(tokptr + lrlen), braces)) {    /* followed by a 1-char brace */
                    strsqueeze((tokptr + 1), (lrlen - 1));      /*so squeeze out "left" or "right" */
                    expptr = tokptr + 2;
                } /* and resume search past brace */
                else {          /* may be a "long" brace like \| */

                    expptr = tokptr + lrlen;    /*init to resume search past\left\rt */
                    for (isymbol = 0; (lrsym = lrfrom[isymbol]) != NULL;
                         isymbol++) {
                        int symlen = strlen(lrsym);     /* #chars in delim, e.g., 2 for \| */
                        if (memcmp(tokptr + lrlen, lrsym, symlen) == 0) {       /* found long delim */
                            strsqueeze((tokptr + 1), (lrlen + symlen - 2));     /*squeeze out delim */
                            *(tokptr + 1) = *(lrto[isymbol]);   /* last char now 1-char delim */
                            expptr = tokptr + 2 - lrlen;        /* resume search past 1-char delim */
                            break;
                        }
                    }
                }
            }
        }
    }
    /* -------------------------------------------------------------------------
       run thru table, converting all {a+b\atop c+d} to \atop{a+b}{c+d}
       -------------------------------------------------------------------------- */
    for (isymbol = 0; (atopsym = atopcommands[isymbol]) != NULL; isymbol++) {
        int atoplen = strlen(atopsym);  /* #chars in \atop */
        expptr = expression;    /* re-start search at beginning */
        while ((tokptr = strstr(expptr, atopsym)) != NULL) {    /* found another atop */
            char *leftbrace = NULL, *rightbrace = NULL; /*ptr to opening {, closing } */
            char termchar = *(tokptr + atoplen);        /* \atop followed by terminator */
            if (msgfp != NULL && msglevel >= 999) {
                fprintf(msgfp, "mimeprep> offset=%d rhs=\"%s\"\n",
                        (int) (tokptr - expression), tokptr);
                fflush(msgfp);
            }
            if (isalpha((int) termchar)) {      /*we just have prefix of longer sym */
                expptr = tokptr + atoplen;      /* just resume search after prefix */
                continue;
            }                   /* but don't process it */
            leftbrace = findbraces(expression, tokptr); /* find left { */
            rightbrace = findbraces(NULL, tokptr + atoplen - 1);        /* find right } */
            if (leftbrace == NULL || rightbrace == NULL) {
                expptr += atoplen;
                continue;
            } /* skip command if didn't find */
            else {              /* we have bracketed { \atop } */

                int leftlen = (int) (tokptr - leftbrace) - 1,   /* #chars in left arg */
                    rightlen = (int) (rightbrace - tokptr) - atoplen,   /* and in right */
                    totlen = (int) (rightbrace - leftbrace) + 1;        /*tot in { \atop } */
                const char *open = atopdelims[2 * isymbol];
                const char *close = atopdelims[2 * isymbol + 1];
                char arg[8192], command[8192];  /* left/right args, new \atop{}{} */
                *command = '\0';        /* start with null string */
                if (open != NULL)
                    strcat(command, open);      /* add open delim if needed */
                strcat(command, atopsym);       /* add command with \atop */
                arg[0] = '{';   /* arg starts with { */
                memcpy(arg + 1, leftbrace + 1, leftlen);        /* extract left-hand arg */
                arg[leftlen + 1] = '\0';        /* and null terminate it */
                strcat(command, arg);   /* concatanate {left-arg to \atop */
                strcat(command, "}{");  /* close left-arg, open right-arg */
                memcpy(arg, tokptr + atoplen, rightlen);        /* right-hand arg */
                arg[rightlen] = '}';    /* add closing } */
                arg[rightlen + 1] = '\0';       /* and null terminate it */
                if (isthischar(*arg, WHITEMATH)) {      /* 1st char was mandatory space */
                    strsqueeze(arg, 1);
                }               /* so squeeze it out */
                strcat(command, arg);   /* concatanate right-arg} */
                if (close != NULL)
                    strcat(command, close);     /* add close delim if needed */
                strchange(totlen - 2, leftbrace + 1, command);  /* {\atop} --> {\atop{}{}} */
                expptr = leftbrace + strlen(command);   /*resume search past \atop{}{} */
            }
        }
    }
/* -------------------------------------------------------------------------
back to caller with preprocessed expression
-------------------------------------------------------------------------- */
    if (msgfp != NULL && msglevel >= 99) {      /* display preprocessed expression */
        fprintf(msgfp, "mimeprep> expression=\"\"%s\"\"\n", expression);
        fflush(msgfp);
    }
    return expression;
}

/* ==========================================================================
 * Function:    strchange ( int nfirst, char *from, char *to )
 * Purpose: Changes the nfirst leading chars of `from` to `to`.
 *      For example, to change char x[99]="12345678" to "123ABC5678"
 *      call strchange(1,x+3,"ABC")
 * --------------------------------------------------------------------------
 * Arguments:   nfirst (I)  int containing #leading chars of `from`
 *              that will be replace by `to`
 *      from (I/O)  char * to null-terminated string whose nfirst
 *              leading chars will be replaced by `to`
 *      to (I)      char * to null-terminated string that will
 *              replace the nfirst leading chars of `from`
 * --------------------------------------------------------------------------
 * Returns: ( char * )  ptr to first char of input `from`
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o If strlen(to)>nfirst, from must have memory past its null
 *      (i.e., we don't do a realloc)
 * ======================================================================= */
static char *strchange(int nfirst, char *from, const char *to) {
    int tolen = (to == NULL ? 0 : strlen(to)),  /* #chars in replacement string */
        nshift = abs(tolen - nfirst);   /*need to shift from left or right */
/* -------------------------------------------------------------------------
shift from left or right to accommodate replacement of its nfirst chars by to
-------------------------------------------------------------------------- */
    if (tolen < nfirst) {       /* shift left is easy */
        strsqueeze(from, nshift);
    } /* memmove avoids overlap memory */ if (tolen > nfirst) { /* need more room at start of from */
        char *pfrom = from + strlen(from);      /* ptr to null terminating from */
        for (; pfrom >= from; pfrom--)  /* shift all chars including null */
            *(pfrom + nshift) = *pfrom;
    }
    /* shift chars nshift places right */
    /* -------------------------------------------------------------------------
       from has exactly the right number of free leading chars, so just put to there
       -------------------------------------------------------------------------- */
    if (tolen != 0)             /* make sure to not empty or null */
        memcpy(from, to, tolen);        /* chars moved into place */
    return from;
}

/* ==========================================================================
 * Function:    strreplace (char *string, char *from, char *to, int nreplace)
 * Purpose: Changes the first nreplace occurrences of 'from' to 'to'
 *      in string, or all occurrences if nreplace=0.
 * --------------------------------------------------------------------------
 * Arguments:   string (I/0)    char * to null-terminated string in which
 *              occurrence of 'from' will be replaced by 'to'
 *      from (I)    char * to null-terminated string
 *              to be replaced by 'to'
 *      to (I)      char * to null-terminated string that will
 *              replace 'from'
 *      nreplace (I)    int containing (maximum) number of
 *              replacements, or 0 to replace all.
 * --------------------------------------------------------------------------
 * Returns: ( int )     number of replacements performed,
 *              or 0 for no replacements or -1 for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
int strreplace(char *string, const char *from, const char *to,
               int nreplace) {
    int fromlen = (from == NULL ? 0 : strlen(from)),    /* #chars to be replaced */
        tolen = (to == NULL ? 0 : strlen(to));  /* #chars in replacement string */
    char *pfrom = NULL;         /*ptr to 1st char of from in string */
    char *pstring = string;     /*ptr past previously replaced from */
    int nreps = 0;              /* #replacements returned to caller */
/* -------------------------------------------------------------------------
repace occurrences of 'from' in string to 'to'
-------------------------------------------------------------------------- */
    if (string == (char *) NULL /* no input string */
        || (fromlen < 1 && nreplace <= 0))      /* replacing empty string forever */
        nreps = (-1);           /* so signal error */
    else                        /* args okay */
        while (nreplace < 1 || nreps < nreplace) {      /* up to #replacements requested */
            if (fromlen > 0)    /* have 'from' string */
                pfrom = strstr(pstring, from);  /*ptr to 1st char of from in string */
            else
                pfrom = pstring;        /*or empty from at start of string */
            if (pfrom == NULL)
                break;          /*no more from's, so back to caller */
            if (strchange(fromlen, pfrom, to)   /* leading 'from' changed to 'to' */
                == NULL) {
                nreps = -1;
                break;
            } /* signal error to caller */ nreps++;     /* count another replacement */
            pstring = pfrom + tolen;    /* pick up search after 'to' */
            if (*pstring == '\0')
                break;          /* but quit at end of string */
        }                       /* --- end-of-while() --- */
    return (nreps);             /* #replacements back to caller */
}                               /* --- end-of-function strreplace() --- */


/* ==========================================================================
 * Function:    strwstr (char *string, char *substr, char *white, int *sublen)
 * Purpose: Find first substr in string, but wherever substr contains
 *      a whitespace char (in white), string may contain any number
 *      (including 0) of whitespace chars. If white contains I or i,
 *      then match is case-insensitive (and I,i _not_ whitespace).
 * --------------------------------------------------------------------------
 * Arguments:   string (I)  char * to null-terminated string in which
 *              first occurrence of substr will be found
 *      substr (I)  char * to null-terminated string containing
 *              "template" that will be searched for
 *      white (I)   char * to null-terminated string containing
 *              whitespace chars.  If NULL or empty, then
 *              "~ \t\n\r\f\v" (WHITEMATH in mimetex.h) used.
 *              If white contains I or i, then match is
 *              case-insensitive (and I,i _not_ considered
 *              whitespace).
 *      sublen (O)  address of int returning "length" of substr
 *              found in string (which may be longer or
 *              shorter than substr itself).
 * --------------------------------------------------------------------------
 * Returns: ( char * )  ptr to first char of substr in string
 *              or NULL if not found or for any error.
 * --------------------------------------------------------------------------
 * Notes:     o Wherever a single whitespace char appears in substr,
 *      the corresponding position in string may contain any
 *      number (including 0) of whitespace chars, e.g.,
 *      string="abc   def" and string="abcdef" both match
 *      substr="c d" at offset 2 of string.
 *        o If substr="c  d" (two spaces between c and d),
 *      then string must have at least one space, so now "abcdef"
 *      doesn't match.  In general, the minimum number of spaces
 *      in string is the number of spaces in substr minus 1
 *      (so 1 space in substr permits 0 spaces in string).
 *        o Embedded spaces are counted in sublen, e.g.,
 *      string="c   d" (three spaces) matches substr="c d"
 *      with sublen=5 returned.  But string="ab   c   d" will
 *      also match substr="  c d" returning sublen=5 and
 *      a ptr to the "c".  That is, the mandatory preceding
 *      space is _not_ counted as part of the match.
 *      But all the embedded space is counted.
 *      (An inconsistent bug/feature is that mandatory
 *      terminating space is counted.)
 *        o Moreover, string="c   d" matches substr="  c d", i.e.,
 *      the very beginning of a string is assumed to be preceded
 *      by "virtual blanks".
 * ======================================================================= */
char *strwstr(const char *string, const char *substr, const char *white,
              int *sublen) {
    const char *psubstr = substr;
    const char *pstring = string;       /*ptr to current char in substr,str */
    const char *pfound = NULL;  /*ptr to found substr back to caller */
    char *pwhite = NULL;
    char whitespace[256];       /* callers white whithout i,I */
    int iscase = (white == NULL || (strchr(white, 'i') == NULL && strchr(white, 'I') == NULL)); /* case-sensitive if i,I in white */
    int foundlen = 0;           /* length of substr found in string */
    int nstrwhite = 0;
    int nsubwhite = 0;          /* #leading white chars in str,sub */
    int nminwhite = 0;          /* #mandatory leading white in str */
    int nstrchars = 0;
    int nsubchars = 0;          /* #non-white chars to be matched */
    int isncmp = 0;             /*strncmp() or strncasecmp() result */
/* -------------------------------------------------------------------------
Initialization
-------------------------------------------------------------------------- */
/* --- set up whitespace --- */
     strcpy(whitespace, WHITEMATH);     /*default if no user input for white */
    if (white != NULL) {        /*user provided ptr to white string */
        if (*white != '\0') {   /*and it's not just an empty string */
            strcpy(whitespace, white);  /* so use caller's white spaces */
            while ((pwhite = strchr(whitespace, 'i')) != NULL) {        /* have an embedded i */
                strsqueeze(pwhite, 1);
            }
            while ((pwhite = strchr(whitespace, 'I')) != NULL) {        /* have an embedded I */
                strsqueeze(pwhite, 1);
            }
            if (*whitespace == '\0') {  /* caller's white just had i,I */
                strcpy(whitespace, WHITEMATH);
            }
        }
    }
    /* -------------------------------------------------------------------------
       Find first occurrence of substr in string
       -------------------------------------------------------------------------- */
    if (string != NULL) {       /* caller passed us a string ptr */
        while (*pstring != '\0') {      /* break when string exhausted */
            const char *pstrptr = pstring;      /* (re)start at next char in string */
            int leadingwhite = 0;       /* leading whitespace */
            psubstr = substr;   /* start at beginning of substr */
            foundlen = 0;       /* reset length of found substr */
            if (substr != NULL) /* caller passed us a substr ptr */
                while (*psubstr != '\0') {      /*see if pstring begins with substr */
                    /* --- check for end-of-string before finding match --- */
                    if (*pstrptr == '\0') {     /* end-of-string without a match */
                        goto nextstrchar;       /* keep trying with next char */
                    }
                    /* --- actual amount of whitespace in string and substr --- */
                    nsubwhite = strspn(psubstr, whitespace);    /* #leading white chars in sub */
                    nstrwhite = strspn(pstrptr, whitespace);    /* #leading white chars in str */
                    nminwhite = max2(0, nsubwhite - 1); /* #mandatory leading white in str */
                    /* --- check for mandatory leading whitespace in string --- */
                    if (pstrptr != string)      /*not mandatory at start of string */
                        if (nstrwhite < nminwhite)      /* too little leading white space */
                            goto nextstrchar;   /* keep trying with next char */
                    /* ---hold on to #whitespace chars in string preceding substr match--- */
                    if (pstrptr == pstring)     /* whitespace at start of substr */
                        leadingwhite = nstrwhite;       /* save it as leadingwhite */
                    /* --- check for optional whitespace --- */
                    if (psubstr != substr)      /* always okay at start of substr */
                        if (nstrwhite > 0 && nsubwhite < 1)     /* too much leading white space */
                            goto nextstrchar;   /* keep trying with next char */
                    /* --- skip any leading whitespace in substr and string --- */
                    psubstr += nsubwhite;       /* push past leading sub whitespace */
                    pstrptr += nstrwhite;       /* push past leading str whitespace */
                    /* --- now get non-whitespace chars that we have to match --- */
                    nsubchars = strcspn(psubstr, whitespace);   /* #non-white chars in sub */
                    nstrchars = strcspn(pstrptr, whitespace);   /* #non-white chars in str */
                    if (nstrchars < nsubchars)  /* too few chars for match */
                        goto nextstrchar;       /* keep trying with next char */
                    /* --- see if next nsubchars are a match --- */
                    isncmp = (iscase ? strncmp(pstrptr, psubstr, nsubchars) :   /*case sensitive */
                              strncasecmp(pstrptr, psubstr, nsubchars));        /*case insensitive */
                    if (isncmp != 0)    /* no match */
                        goto nextstrchar;       /* keep trying with next char */
                    /* --- push past matched chars --- */
                    psubstr += nsubchars;
                    pstrptr += nsubchars;       /*nsubchars were matched */
                }               /* --- end-of-while(*psubstr!='\0') --- */
            pfound = pstring + leadingwhite;    /* found match starting at pstring */
            foundlen = (int) (pstrptr - pfound);        /* consisting of this many chars */
            goto end_of_job;    /* back to caller */
            /* ---failed to find substr, continue trying with next char in string--- */
          nextstrchar:         /* continue outer loop */
            pstring++;          /* bump to next char in string */
        }
    }
    /* -------------------------------------------------------------------------
       Back to caller with ptr to first occurrence of substr in string
       -------------------------------------------------------------------------- */
  end_of_job:
    if (msglevel >= 999 && msgfp != NULL) {     /* debugging/diagnostic output */
        fprintf(msgfp,
                "strwstr> str=\"%.72s\" sub=\"%s\" found at offset %d\n",
                string, substr,
                (pfound == NULL ? (-1) : (int) (pfound - string)));
        fflush(msgfp);
    }
    if (sublen != NULL) {       /*caller wants length of found substr */
        *sublen = foundlen;     /* give it to him along with ptr */
    }
    return (char *) pfound;     /*ptr to first found substr, or NULL */
}


/* ==========================================================================
 * Function:    strdetex ( s, mode )
 * Purpose: Removes/replaces any LaTeX math chars in s
 *      so that s can be displayed "verbatim",
 *      e.g., for error messages.
 * --------------------------------------------------------------------------
 * Arguments:   s (I)       char * to null-terminated string
 *              whose math chars are to be removed/replaced
 *      mode (I)    int containing 0 to _not_ use macros (i.e.,
 *              mimeprep won't be called afterwards),
 *              or containing 1 to use macros that will
 *              be expanded by a subsequent call to mimeprep.
 * --------------------------------------------------------------------------
 * Returns: ( char * )  ptr to "cleaned" copy of s
 *              or "" (empty string) for any error.
 * --------------------------------------------------------------------------
 * Notes:     o The returned pointer addresses a static buffer,
 *      so don't call strdetex() again until you're finished
 *      with output from the preceding call.
 * ======================================================================= */
static char *strdetex(char *s, int mode) {
    static char sbuff[4096];    /* copy of s with no math chars */
/* -------------------------------------------------------------------------
Make a clean copy of s
-------------------------------------------------------------------------- */
/* --- check input --- */
    *sbuff = '\0';              /* initialize in case of error */
    if (isempty(s)) {
        goto end_of_job;        /* no input */
    }
                                                                        /* --- start with copy of s --- */ strninit(sbuff, s, 2048);
                                                                        /* leave room for replacements */
/* --- make some replacements -- we *must* replace \ { } first --- */
    strreplace(sbuff, "\\", "\\backslash~\\!\\!", 0);   /*change all \'s to text */
    strreplace(sbuff, "{", "\\lbrace~\\!\\!", 0);       /*change all {'s to \lbrace */
    strreplace(sbuff, "}", "\\rbrace~\\!\\!", 0);       /*change all }'s to \rbrace */
/* --- now our further replacements may contain \directives{args} --- */
    if (mode >= 1)
        strreplace(sbuff, "_", "\\_", 0);       /* change all _'s to \_ */
    else
        strreplace(sbuff, "_", "\\underline{\\qquad}", 0);      /*change them to text */
    if (0)
        strreplace(sbuff, "<", "\\textlangle ", 0);     /* change all <'s to text */
    if (0)
        strreplace(sbuff, ">", "\\textrangle ", 0);     /* change all >'s to text */
    if (0)
        strreplace(sbuff, "$", "\\textdollar ", 0);     /* change all $'s to text */
    strreplace(sbuff, "$", "\\$", 0);   /* change all $'s to \$ */
    strreplace(sbuff, "&", "\\&", 0);   /* change all &'s to \& */
    strreplace(sbuff, "%", "\\%", 0);   /* change all %'s to \% */
    strreplace(sbuff, "#", "\\#", 0);   /* change all #'s to \# */
    /*strreplace(sbuff,"~","\\~",0); *//* change all ~'s to \~ */
    strreplace(sbuff, "^", "{\\fs{+2}\\^}", 0); /* change all ^'s to \^ */
  end_of_job:
    return sbuff;
}

/* ==========================================================================
 * Function:    strtexchr ( char *string, char *texchr )
 * Purpose: Find first texchr in string, but texchr must be followed
 *      by non-alpha
 * --------------------------------------------------------------------------
 * Arguments:   string (I)  char * to null-terminated string in which
 *              first occurrence of delim will be found
 *      texchr (I)  char * to null-terminated string that
 *              will be searched for
 * --------------------------------------------------------------------------
 * Returns: ( char * )  ptr to first char of texchr in string
 *              or NULL if not found or for any error.
 * --------------------------------------------------------------------------
 * Notes:     o texchr should contain its leading \, e.g., "\\left"
                                                                              * ======================================================================= *//* --- entry point --- */
char *strtexchr(const char *string, const char *texchr) {
    char delim, *ptexchr = NULL;        /* ptr returned to caller */
    const char *pstring = string;       /* start or continue up search here */
    int texchrlen = (texchr == NULL ? 0 : strlen(texchr));      /* #chars in texchr */
/* -------------------------------------------------------------------------
locate texchr in string
-------------------------------------------------------------------------- */
    if (string != NULL && texchrlen > 0) {
        /* check that we got input string */
        /* and a texchr to search for */
        while ((ptexchr = strstr(pstring, texchr)) != NULL) {   /* look for texchr in string */
            if ((delim = ptexchr[texchrlen]) == '\0') { /* char immediately after texchr */
                break;
            } else if (isalpha(delim)) {
                /* if there are chars after texchr */
                /*texchr is prefix of longer symbol */
                /* other tests to be determined */
                pstring = ptexchr + texchrlen;  /* continue search after texchr */
            } else {            /* passed all tests */
                break;
            }
        }
    }
    return (char *) ptexchr;    /* ptr to texchar back to caller */
}


/* ==========================================================================
 * Function:    findbraces ( char *expression, char *command )
 * Purpose: If expression!=NULL, finds opening left { preceding command;
 *      if expression==NULL, finds closing right } after command.
 *      For example, to parse out {a+b\over c+d} call findbraces()
 *      twice.
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  NULL to find closing right } after command,
 *              or char * to null-terminated string to find
 *              left opening { preceding command.
 *      command (I) char * to null-terminated string whose
 *              first character is usually the \ of \command
 * --------------------------------------------------------------------------
 * Returns: ( char * )  ptr to either opening { or closing },
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static char *findbraces(const char *expression, const char *command) {
    int isopen = (expression == NULL ? 0 : 1);  /* true to find left opening { */
    const char *left = "{";
    const char *right = "}";    /* delims bracketing {x\command y} */
    const char *delim = (isopen ? left : right);        /* delim we want,  { if isopen */
    const char *match = (isopen ? right : left);        /* matching delim, } if isopen */
    const char *brace = NULL;   /* ptr to delim returned to caller */
    int inc = (isopen ? -1 : +1);       /* pointer increment */
    int level = 1;              /* nesting level, for {{}\command} */
    const char *ptr = command;  /* start search here */
    int setbrace = 1;           /* true to set {}'s if none found */
/* -------------------------------------------------------------------------
search for left opening { before command, or right closing } after command
-------------------------------------------------------------------------- */
    while (1) {                 /* search for brace, or until end */
        /* --- next char to check for delim --- */
        ptr += inc;             /* bump ptr left or right */
        /* --- check for beginning or end of expression --- */
        if (isopen) {           /* going left, check for beginning */
            if (ptr < expression) {
                break;
            }
        } else {
            /* went before start of string */
            if (*ptr == '\0') {
                break;
            }
        }                       /* went past end of string */
        /* --- don't check this char if it's escaped --- */
        if (!isopen || ptr > expression) {      /* very first char can't be escaped */
            if (isthischar(*(ptr - 1), ESCAPE)) {       /* escape char precedes current */
                continue;       /* so don't check this char */
            }
        }
        /* --- check for delim --- */
        if (isthischar(*ptr, delim))    /* found delim */
            if (--level == 0) { /* and it's not "internally" nested */
                brace = ptr;    /* set ptr to brace */
                goto end_of_job;
            }
        /* and return it to caller */
        /* --- check for matching delim --- */
        if (isthischar(*ptr, match)) {  /* found matching delim */
            level++;            /* so bump nesting level */
        }
    }
  end_of_job:
    if (brace == NULL) {        /* open{ or close} not found */
        if (setbrace) {         /* want to force one at start/end? */
            brace = ptr;        /* { before expressn, } after cmmnd */
        }
    }
    return (char *) brace;      /*back to caller with delim or NULL */
}

/* ==========================================================================
 * Function:    strpspn ( char *s, char *reject, char *segment )
 * Purpose: finds the initial segment of s containing no chars
 *      in reject that are outside (), [] and {} parens, e.g.,
 *         strpspn("abc(---)def+++","+-",segment) returns
 *         segment="abc(---)def" and a pointer to the first '+' in s
 *      because the -'s are enclosed in () parens.
 * --------------------------------------------------------------------------
 * Arguments:   s (I)       (char *)pointer to null-terminated string
 *              whose initial segment is desired
 *      reject (I)  (char *)pointer to null-terminated string
 *              containing the "reject chars"
 *      segment (O) (char *)pointer returning null-terminated
 *              string comprising the initial segment of s
 *              that contains non-rejected chars outside
 *              (),[],{} parens, i.e., all the chars up to
 *              but not including the returned pointer.
 *              (That's the entire string if no non-rejected
 *              chars are found.)
 * --------------------------------------------------------------------------
 * Returns: ( char * )  pointer to first reject-char found in s
 *              outside parens, or a pointer to the
 *              terminating '\0' of s if there are
 *              no reject chars in s outside all () parens.
 * --------------------------------------------------------------------------
 * Notes:     o the return value is _not_ like strcspn()'s
 *        o improperly nested (...[...)...] are not detected,
 *      but are considered "balanced" after the ]
 *        o if reject not found, segment returns the entire string s
 *        o leading/trailing whitespace is trimmed from returned segment
 * ======================================================================= */
char *strpspn(const char *s, const char *reject, char *segment) {
    const char *ps = s;         /* current pointer into s */
    int depth = 0;              /* () paren nesting level */
    int seglen = 0;
    int maxseg = 2047;          /* segment length, max allowed */
/* -------------------------------------------------------------------------
initialization
-------------------------------------------------------------------------- */
/* --- check arguments --- */
    if (isempty(s)) {           /* no input string supplied */
        goto end_of_job;        /* no reject chars supplied */
    }
/* -------------------------------------------------------------------------
find first char from s outside () parens (and outside ""'s) and in reject
                                                                                                        -------------------------------------------------------------------------- */ while (*ps != '\0') {
                                                                                                        /* search till end of input string */
        if (isthischar(*ps, "([{")) {
            depth++;            /* push another paren */
        }
        if (isthischar(*ps, ")]}")) {
            depth--;            /* or pop another paren */
        }
        if (depth < 1) {        /* we're outside all parens */
            if (isempty(reject)) {
                break;          /* no reject so break immediately */
            }
            if (isthischar(*ps, reject)) {
                break;
            }
        }
        /* only break on a reject char */
        if (segment != NULL) {  /* caller gave us segment */
            if (seglen < maxseg) {      /* don't overflow segment buffer */
                memcpy(segment + seglen, ps, 1);        /* so copy non-reject char */
            }
        }
        seglen += 1;
        ps += 1;                /* bump to next char */
    }                           /* --- end-of-while(*ps!=0) --- */
  end_of_job:
    if (segment != NULL) {      /* caller gave us segment */
        if (isempty(reject)) {  /* no reject char */
            segment[min2(seglen, maxseg)] = *ps;
            seglen++;
        }                       /*closing )]} to seg */
        segment[min2(seglen, maxseg)] = '\0';   /* null-terminate the segment */
        trimwhite(segment);
    }                           /* trim leading/trailing whitespace */
    return (char *) ps;         /* back to caller */
}

/* ==========================================================================
 * Function:    isstrstr ( char *string, char *snippets, int iscase )
 * Purpose: determine whether any substring of 'string'
 *      matches any of the comma-separated list of 'snippets',
 *      ignoring case if iscase=0.
 * --------------------------------------------------------------------------
 * Arguments:   string (I)  char * containing null-terminated
 *              string that will be searched for
 *              any one of the specified snippets
 *      snippets (I)    char * containing null-terminated,
 *              comma-separated list of snippets
 *              to be searched for in string
 *      iscase (I)  int containing 0 for case-insensitive
 *              comparisons, or 1 for case-sensitive
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if any snippet is a substring of
 *              string, 0 if not
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
int isstrstr(const char *string, const char *snippets, int iscase)
{
    int status = 0;             /*1 if any snippet found in string */
    char snip[256];
    const char *snipptr = snippets;     /* munge through each snippet */
    char delim = ',';
    char *delimptr = NULL;      /* separated by delim's */
    char stringcp[4096];
    char *cp = stringcp;        /*maybe lowercased copy of string */

/* --- arg check --- */
    if (string == NULL || snippets == NULL) {
        goto end_of_job;        /* missing arg */
    }
    if (*string == '\0' || *snippets == '\0') {
        goto end_of_job;        /* empty arg */
    }
/* --- copy string and lowercase it if case-insensitive --- */
    strninit(stringcp, string, 4064);   /* local copy of string */
    if (!iscase) {              /* want case-insensitive compares */
        for (cp = stringcp; *cp != '\0'; cp++) {        /* so for each string char */
            if (isupper(*cp)) {
                *cp = tolower(*cp);     /*lowercase any uppercase chars */
            }
        }
    }
/* -------------------------------------------------------------------------
extract each snippet and see if it's a substring of string
-------------------------------------------------------------------------- */
    while (snipptr != NULL) {   /* while we still have snippets */
        /* --- extract next snippet --- */
        if ((delimptr = strchr(snipptr, delim)) /* locate next comma delim */
            == NULL) {          /*not found following last snippet */
            strninit(snip, snipptr, 255);       /* local copy of last snippet */
            snipptr = NULL;
        } /* signal end-of-string */
        else {                  /* snippet ends just before delim */
            int sniplen = (int) (delimptr - snipptr) - 1;       /* #chars in snippet */
            memcpy(snip, snipptr, sniplen);     /* local copy of snippet chars */
            snip[sniplen] = '\0';       /* null-terminated snippet */
            snipptr = delimptr + 1;
        }                       /* next snippet starts after delim */
        /* --- lowercase snippet if case-insensitive --- */
        if (!iscase)            /* want case-insensitive compares */
            for (cp = snip; *cp != '\0'; cp++)  /* so for each snippet char */
                if (isupper(*cp))
                    *cp = tolower(*cp); /*lowercase any uppercase chars */
        /* --- check if snippet in string --- */
        if (strstr(stringcp, snip) != NULL) {   /* found snippet in string */
            status = 1;         /* so reset return status */
            break;
        }                       /* no need to check any further */
    }                           /* --- end-of-while(*snipptr!=0) --- */
  end_of_job:
    return (status);  /*1 if snippet found in list, else 0 */
}


/* ==========================================================================
 * Function:    isnumeric ( s )
 * Purpose: determine if s is an integer
 * --------------------------------------------------------------------------
 * Arguments:   s (I)       (char *)pointer to null-terminated string
 *              that's checked for a leading + or -
 *              followed by digits
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 if s is numeric, 0 if it is not
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static int isnumeric(const char *s)
{
    int status = 0;             /* return 0 if not numeric, 1 if is */
    const char *p = s;          /* pointer into s */
    if (isempty(s)) {
        goto end_of_job;        /* missing arg or empty string */
    }
    skipwhite(p);               /*check for leading +or- after space */
    if (*p == '+' || *p == '-') {
        p++;                    /* skip leading + or - */
    }
    for (; *p != '\0'; p++) {   /* check rest of s for digits */
        if (isdigit(*p)) {
            continue;           /* still got uninterrupted digits */
        }
        if (!isthischar(*p, WHITESPACE)) {
            goto end_of_job;    /* non-numeric char */
        }
        skipwhite(p);           /* skip all subsequent whitespace */
        if (*p == '\0') {
            break;              /* trailing whitespace okay */
        }
        goto end_of_job;        /* embedded whitespace non-numeric */
    }
    status = 1;

  end_of_job:
    return status;
}

/* ==========================================================================
 * Function:    evalterm ( STORE *store, char *term )
 * Purpose: evaluates a term
 * --------------------------------------------------------------------------
 * Arguments:   store (I/O) STORE * containing environment
 *              in which term is to be evaluated
 *      term (I)    char * containing null-terminated string
 *              with a term like "3" or "a" or "a+3"
 *              whose value is to be determined
 * --------------------------------------------------------------------------
 * Returns: ( int )     value of term,
 *              or NOVALUE for any error
 * --------------------------------------------------------------------------
 * Notes:     o Also evaluates index?a:b:c:etc, returning a if index<=0,
 *      b if index=1, etc, and the last value if index is too large.
 *      Each a:b:c:etc can be another expression, including another
 *      (index?a:b:c:etc) which must be enclosed in parentheses.
 * ======================================================================= */

static int evalterm(STORE *store, const char *term)
{
    int termval = 0;            /* term value returned to caller */
    char token[2048] = "\0";  /* copy term */
    char *delim = NULL;         /* delim '(' or '?' in token */
    static int evaltermdepth = 0;       /* recursion depth */
    int novalue = (-89123456);  /* dummy (for now) error signal */

    if (++evaltermdepth > 99) {
         goto end_of_job;       /*probably recursing forever */
    }
    if (store == NULL || isempty(term)) {
        goto end_of_job;        /*check for missing arg */
    }
    skipwhite(term);           /* skip any leading whitespace */
/* -------------------------------------------------------------------------
First look for conditional of the form term?term:term:...
-------------------------------------------------------------------------- */
/* ---left-hand part of conditional is chars preceding "?" outside ()'s--- */
    delim = strpspn(term, "?", token); /* chars preceding ? outside () */
    if (*delim != '\0') {       /* found conditional expression */
        int ncolons = 0;        /* #colons we've found so far */
        if (*token != '\0')     /* evaluate "index" value on left */
            if ((termval = evalterm(store, token))      /* evaluate left-hand term */
                == novalue)
                goto end_of_job;        /* return error if failed */
        while (*delim != '\0') {        /* still have chars in term */
            delim++;
            *token = '\0';      /* initialize for next "value:" */
            if (*delim == '\0')
                break;          /* no more values */
            delim = strpspn(delim, ":", token); /* chars preceding : outside () */
            if (ncolons++ >= termval) {
                break;          /* have corresponding term */
            }
        }                       /* --- end-of-while(*delim!='\0')) --- */
        if (*token != '\0') {    /* have x:x:value:x:x on right */
             termval = evalterm(store, token);  /* so evaluate it */
        }
        goto end_of_job;        /* return result to caller */
    }

    /* -------------------------------------------------------------------------
       evaluate a+b recursively
       -------------------------------------------------------------------------- */
    /* --- left-hand part of term is chars preceding "/+-*%" outside ()'s --- */
    term = strpspn(term, "/+-*%", token);       /* chars preceding /+-*% outside () */
/* --- evaluate a+b, a-b, etc --- */
    if (*term != '\0') {        /* found arithmetic operation */
        int leftval = 0, rightval = 0;  /* init leftval for unary +a or -a */
        if (*token != '\0')     /* or eval for binary a+b or a-b */
            if ((leftval = evalterm(store, token)) == novalue) {
                goto end_of_job;        /* return error if failed */
            }
        if ((rightval = evalterm(store, term + 1)) == novalue) {
            goto end_of_job;    /* return error if failed */
        }
        switch (*term) {        /* perform requested arithmetic */
        default:
            break;              /* internal error */
        case '+':
            termval = leftval + rightval;
            break;              /* addition */
        case '-':
            termval = leftval - rightval;
            break;              /* subtraction */
        case '*':
            termval = leftval * rightval;
            break;              /* multiplication */
        case '/':
            if (rightval != 0) {  /* guard against divide by zero */
                termval = leftval / rightval;
            }
            break;              /* integer division */
        case '%':
            if (rightval != 0) {  /* guard against divide by zero */
                termval = leftval % rightval;
            }
            break;              /*left modulo right */
        }                       /* --- end-of-switch(*relation) --- */
        goto end_of_job;        /* return result to caller */
    }
    /* -------------------------------------------------------------------------
       check for parenthesized expression or term of the form function(arg1,arg2,...)
       -------------------------------------------------------------------------- */
    if ((delim = strchr(token, '(')) != NULL) { /* token contains a ( */
        /* --- strip trailing paren (if there hopefully is one) --- */
        int toklen = strlen(token);     /* total #chars in token */
        if (token[toklen - 1] == ')') {   /* found matching ) at end of token */
            token[--toklen] = '\0';     /* remove trailing ) */
        }
        /* --- handle parenthesized subexpression --- */
        if (*token == '(') {    /* have parenthesized expression */
            strsqueeze(token, 1);       /* so squeeze out leading ( */
            /* --- evaluate edited term --- */
            trimwhite(token);   /* trim leading/trailing whitespace */
            termval = evalterm(store, token);
        } else {                  /* have function(arg1,arg2,...) */
            *delim = '\0';      /* separate function name and args */
            /*termval = evalfunc(store,token,delim+1); */
        }
        /* evaluate function */
        goto end_of_job;
    }
    /* return result to caller */
    /* -------------------------------------------------------------------------
       evaluate constants directly, or recursively evaluate variables, etc
       -------------------------------------------------------------------------- */
    if (*token != '\0') {       /* empty string */
        if (isnumeric(token)) {  /* have a constant */
            termval = atoi(token);      /* convert ascii-to-int */
        } else {                  /* variable or "stored proposition" */
            termval = getstore(store, token);
        }                       /* look up token */
    }
    /* -------------------------------------------------------------------------
       back to caller with truth value of proposition
       -------------------------------------------------------------------------- */
  end_of_job:
    /* --- back to caller --- */
    if (evaltermdepth > 0) {
        evaltermdepth--;        /* pop recursion depth */
    }
    return termval;
}

/* ==========================================================================
 * Function:    getstore ( store, identifier )
 * Purpose: finds identifier in store and returns corresponding value
 * --------------------------------------------------------------------------
 * Arguments:   store (I)   (STORE *)pointer to store containing
 *              the desired identifier
 *      identifier (I)  (char *)pointer to null-terminated string
 *              containing the identifier whose value
 *              is to be returned
 * --------------------------------------------------------------------------
 * Returns: ( int )     identifier's corresponding value,
 *              or 0 if identifier not found (or any error)
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static int getstore(STORE * store, const char *identifier)
{
    int value = 0;              /* store[istore].value for identifier */
    int istore = 0;             /* store[] index containing identifier */
    char seek[512], hide[512];  /* identifier arg, identifier in store */

    if (store == NULL || isempty(identifier)) {
        goto end_of_job;
    }
    strninit(seek, identifier, 500);   /* local copy of caller's identifier */
    trimwhite(seek);           /* remove leading/trailing whitespace */

/* --- loop over store --- */
    for (istore = 0; istore < MAXSTORE; istore++) {     /* until end-of-table */
        const char *idstore = store[istore].identifier; /* ptr to identifier in store */
        if (isempty(idstore)) {   /* empty id signals eot */
            break;             /* de-reference any default/error value */
        }
        strninit(hide, idstore, 500);  /* local copy of store[] identifier */
        trimwhite(hide);       /* remove leading/trailing whitespace */
        if (!strcmp(hide, seek)) {        /* found match */
            break;             /* de-reference corresponding value */
        }
    }                           /* --- end-of-for(istore) --- */
    if (store[istore].value != NULL) {   /* address of int supplied */
         value = *(store[istore].value);        /* return de-referenced int */
    }
  end_of_job:
    return value;
}
#endif                          /* PART2 */

#if 1
/* ==========================================================================
 * Function:    rasterize ( expression, size )
 * Purpose: returns subraster corresponding to (a valid LaTeX) expression
 *      at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char * to first char of null-terminated
 *              string containing valid LaTeX expression
 *              to be rasterized
 *      size (I)    int containing 0-4 default font size
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to expression,
 *              or NULL for any parsing error.
 * --------------------------------------------------------------------------
 * Notes:     o This is mimeTeX's "main" reusable entry point.  Easy to use:
 *      just call it with a LaTeX expression, and get back a bitmap
 *      of that expression.  Then do what you want with the bitmap.
 * ======================================================================= */
/* --- entry point --- */

subraster *rasterize(const char *expression, int size)
{
    char pretext[512];          /* process preamble, if present */
    char chartoken[MAXSUBXSZ + 1];      /*get subexpression from expr */
    const char *subexpr = chartoken;    /* token may be parenthesized expr */
    mathchardef *symdef;        /*get mathchardef struct for symbol */
    int ligdef;                 /*get symtable[] index for ligature */
    int natoms = 0;             /* #atoms/tokens processed so far */
    subraster *sp = NULL;
    subraster *prevsp = NULL;   /* raster for current, prev char */
    subraster *expraster = NULL;        /* raster returned to caller */
    int family = fontinfo[fontnum].family;      /* current font family */
    int isleftscript = 0;       /* true if left-hand term scripted */
    int wasscripted = 0;        /* true if preceding token scripted */
    int wasdelimscript = 0;     /* true if preceding delim scripted */
    /*int   pixsz = 1; *//*default #bits per pixel, 1=bitmap */
/* --- global values saved/restored at each recursive iteration --- */
    int wasstring = isstring;   /* initial isstring mode flag */
    int wasdisplaystyle = isdisplaystyle;       /*initial displaystyle mode flag */
    int oldfontnum = fontnum;   /* initial font family */
    int oldfontsize = fontsize; /* initial fontsize */
    int olddisplaysize = displaysize;   /* initial \displaystyle size */
    int oldshrinkfactor = shrinkfactor; /* initial shrinkfactor */
    int oldsmashmargin = smashmargin;   /* initial smashmargin */
    int oldissmashdelta = issmashdelta; /* initial issmashdelta */
    int oldisexplicitsmash = isexplicitsmash;   /* initial isexplicitsmash */
    int oldisscripted = isscripted;     /* initial isscripted */
    int *oldworkingparam = workingparam;        /* initial working parameter */
    subraster *oldworkingbox = workingbox;      /* initial working box */
    subraster *oldleftexpression = leftexpression;      /*left half rasterized so far */
    double oldunitlength = unitlength;  /* initial unitlength */
    mathchardef *oldleftsymdef = leftsymdef;    /* init oldleftsymdef */

    recurlevel++;               /* wind up one more recursion level */
    leftexpression = NULL;      /* no leading left half yet */
    isreplaceleft = 0;          /* reset replaceleft flag */
    if (1) {
        fraccenterline = NOVALUE;       /* reset \frac baseline signal */
    }
    /* shrinkfactor = shrinkfactors[max2(0,min2(size,LARGESTSIZE))]; *//*set sf */
    shrinkfactor = shrinkfactors[max2(0, min2(size, 16))];      /* have 17 sf's */
    rastlift = 0;               /* reset global rastraise() lift */
    if (msgfp != NULL && msglevel >= 9) {       /*display expression for debugging */
        fprintf(msgfp,
                "rasterize> recursion#%d, size=%d,\n\texpression=\"%s\"\n",
                recurlevel, size,
                (expression == NULL ? "<null>" : expression));
        fflush(msgfp);
    }
    if (expression == NULL) {
        goto end_of_job;        /* nothing given to do */
    }
/* -------------------------------------------------------------------------
preocess optional $-terminated preamble preceding expression
-------------------------------------------------------------------------- */
    expression = preamble((char *) expression, &size, pretext); /* size may be modified */
    if (*expression == '\0') {
        goto end_of_job;        /* nothing left to do */
    }
    fontsize = size;            /* start at requested size */
    if (isdisplaystyle == 1) {  /* displaystyle enabled but not set */
        if (!ispreambledollars) {       /* style fixed by $$...$$'s */
            isdisplaystyle = (fontsize >= displaysize ? 2 : 1); /*force at large fontsize */
        }
    }
/* -------------------------------------------------------------------------
build up raster one character (or subexpression) at a time
-------------------------------------------------------------------------- */
    for (;;) {
        /* --- kludge for \= cyrillic ligature --- */
        isligature = 0;         /* no ligature found yet */
        family = fontinfo[fontnum].family;      /* current font family */
        if (family == CYR10) {  /* may have cyrillic \= ligature */
            if ((ligdef = get_ligature(expression, family)) >= 0) {
                /* got some ligature */
                if (memcmp(symtable[ligdef].symbol, "\\=", 2) == 0) {   /* starts with \= */
                    isligature = 1;     /* signal \= ligature */
                }
            }
        }
        /* --- get next character/token or subexpression --- */
        subexprptr = expression;        /* ptr within expression to subexpr */
        expression = texsubexpr(expression, chartoken, 0, LEFTBRACES, RIGHTBRACES, 1, 1);
        subexpr = chartoken;    /* "local" copy of chartoken ptr */
        leftsymdef = NULL;      /* no character identified yet */
        sp = NULL;              /* no subraster yet */
        size = fontsize;        /* in case reset by \tiny, etc */
        /*isleftscript = isdelimscript; *//*was preceding term scripted delim */
        wasscripted = isscripted;       /* true if preceding token scripted */
        wasdelimscript = isdelimscript; /* preceding \right delim scripted */
        if (1) {
            isscripted = 0;     /* no subscripted expression yet */
        }
        isdelimscript = 0;      /* reset \right delim scripted flag */
        /* --- debugging output --- */
        if (msgfp != NULL && msglevel >= 9) {   /* display chartoken for debugging */
            fprintf(msgfp,
                    "rasterize> recursion#%d,atom#%d=\"%s\" (isligature=%d,isleftscript=%d)\n",
                    recurlevel, natoms + 1, chartoken, isligature,
                    isleftscript);
            fflush(msgfp);
        }
        if (expression == NULL && *subexpr == '\0') {   /* no more tokens */
            break;              /* and this token empty */
        }
        if (*subexpr == '\0') {
            break;              /* enough if just this token empty */
        }
        /* --- check for parenthesized subexpression --- */
        if (isbrace(subexpr, LEFTBRACES, 1)) {  /* got parenthesized subexpression */
            if ((sp = rastparen(&subexpr, size, prevsp)) == NULL) {     /* rasterize subexpression */
                continue;
            }
            /* flush it if failed to rasterize */
            /* --- single-character atomic token --- */
        } else if (!isthischar(*subexpr, SCRIPTS)) {    /* scripts handled below */
            /* --- first check for opening $ in \text{ if $n-m$ even} --- */
            if (istextmode && *subexpr == '$' && subexpr[1] == '\0') {  /* we're in \text mode and have an opening $ */
                const char *endptr = NULL;
                char mathexpr[MAXSUBXSZ + 1];   /* $expression$ in \text{ } */
                int exprlen = 0;        /* length of $expression$ */
                int textfontnum = fontnum;      /* current text font number */
                /*if ( (endptr=strrchr(expression,'$')) != NULL ) *//*ptr to closing $ */
                if ((endptr = strchr(expression, '$')) != NULL) {       /* ptr to closing $ */
                    exprlen = (int) (endptr - expression);      /* #chars preceding closing $ */
                } else {        /* no closing $ found */
                    exprlen = strlen(expression);       /* just assume entire expression */
                    endptr = expression + (exprlen - 1);
                }               /*and push expression to '\0' */
                exprlen = min2(exprlen, MAXSUBXSZ);     /* don't overflow mathexpr[] */
                if (exprlen > 0) {      /* have something between $$ */
                    memcpy(mathexpr, expression, exprlen);      /*local copy of math expression */
                    mathexpr[exprlen] = '\0';   /* null-terminate it */
                    fontnum = 0;        /* set math mode */
                    sp = rasterize(mathexpr, size);     /* and rasterize $expression$ */
                    fontnum = textfontnum;
                }               /* set back to text mode */
                expression = endptr + 1;        /* push expression past closing $ */
                /* --- otherwise, look up mathchardef for atomic token in table --- */
            } else if ((leftsymdef = symdef = get_symdef(chartoken)) == NULL) { /* lookup failed */
                char literal[512] = "[?]";      /*display for unrecognized literal */
                int oldfontnum_tmp = fontnum;   /* error display in default mode */
                if (msgfp != NULL && msglevel >= 29) {  /* display unrecognized symbol */
                    fprintf(msgfp,
                            "rasterize> get_symdef() failed for \"%s\"\n",
                            chartoken);
                    fflush(msgfp);
                }
                sp = NULL;      /* init to signal failure */
                if (warninglevel < 1) {
                    continue;   /* warnings not wanted */
                }
                fontnum = 0;    /* reset from \mathbb, etc */
                if (isthischar(*chartoken, ESCAPE)) {   /* we got unrecognized \escape *//* --- so display literal {\rm~[\backslash~chartoken?]} ---  */
                    strcpy(literal, "{\\rm~["); /* init error message token */
                    strcat(literal, strdetex(chartoken, 0));    /* detex the token */
                    strcat(literal, "?]}");
                }               /* add closing ? and brace */
                sp = rasterize(literal, size - 1);      /* rasterize literal token */
                fontnum = oldfontnum_tmp;       /* reset font family */
                if (sp == NULL) {
                    continue;
                }
            } else if (symdef->handler != NULL) {
                /* have a handler for this token */
                int arg1 = symdef->charnum;
                int arg2 = symdef->family;
                int arg3 = symdef->class;
                if ((sp = symdef->handler(&expression, size, prevsp, arg1, arg2, arg3)) == NULL) {
                    continue;
                }
            } else if (!isstring) {     /* --- no handler, so just get subraster for this character --- */
                if (isligature) {       /* found a ligature */
                    expression = subexprptr + strlen(symdef->symbol);   /*push past it */
                }
                if ((sp = get_charsubraster(symdef, size)) == NULL) {   /* get subraster */
                    continue;
                }
            } else {
                /* constructing ascii string */
                const char *symbol = symdef->symbol;    /* symbol for ascii string */
                int symlen = (symbol != NULL ? strlen(symbol) : 0);     /*#chars in symbol */
                if (symlen < 1) {
                    continue;   /* no symbol for ascii string */
                }
                if ((sp = new_subraster(symlen + 1, 1, 8)) == NULL) {   /* subraster for symbol */
                    continue;   /* flush token if malloc failed */
                }
                sp->type = ASCIISTRING; /* set subraster type */
                sp->symdef = symdef;    /* and set symbol definition */
                sp->baseline = 1;       /* default (should be unused) */
                strcpy((char *) sp->image->pixmap, symbol);     /* copy symbol */
            }
        }

        /* --- handle any super/subscripts following symbol or subexpression --- */
        sp = rastlimits(&expression, size, sp);
        isleftscript = (wasscripted || wasdelimscript ? 1 : 0); /*preceding term scripted */
        /* --- debugging output --- */
        if (msgfp != NULL && msglevel >= 9) {   /* display raster for debugging */
            fprintf(msgfp, "rasterize> recursion#%d,atom#%d%s\n",
                    recurlevel, natoms + 1,
                    (sp == NULL ? " = <null>" : "..."));
            if (msglevel >= 9)
                fprintf(msgfp,
                        "  isleftscript=%d is/wasscripted=%d,%d is/wasdelimscript=%d,%d\n",
                        isleftscript, isscripted, wasscripted,
                        isdelimscript, wasdelimscript);
            if (msglevel >= 99) {
                if (sp != NULL) {
                    type_raster(sp->image, msgfp);      /* display raster */
                }
            }
            fflush(msgfp);
        }

        /* --- accumulate atom or parenthesized subexpression --- */
        if (natoms < 1          /* nothing previous to concat */
            || expraster == NULL        /* or previous was complete error */
            || isreplaceleft) { /* or we're replacing previous */
            if (1 && expraster != NULL) /* probably replacing left */
                delete_subraster(expraster);    /* so first free original left */
            expraster = subrastcpy(sp); /* copy static CHARASTER or left */
            isreplaceleft = 0;
        } else if (sp != NULL) {
            /* ...if we have a new term */
            int prevsmashmargin = smashmargin;  /* save current smash margin */
            if (isleftscript) { /* don't smash against scripts */
                isdelimscript = 0;      /* reset \right delim scripted flag */
                if (!isexplicitsmash)
                    smashmargin = 0;
            }                   /* signal no smash wanted */
            expraster = rastcat(expraster, sp, 1);      /* concat new term, free previous */
            smashmargin = prevsmashmargin;
        }                       /* restore current smash margin */
        delete_subraster(prevsp);       /* free prev (if not a CHARASTER) */
        prevsp = sp;            /* current becomes previous */
        leftexpression = expraster;     /* left half rasterized so far */

        natoms++;               /* bump #atoms count */
    }
/* -------------------------------------------------------------------------
back to caller with rasterized expression
-------------------------------------------------------------------------- */
  end_of_job:
    delete_subraster(prevsp);   /* free last (if not a CHARASTER) */
    /* --- debugging output --- */
    if (msgfp != NULL && msglevel >= 999) {     /* display raster for debugging */
        fprintf(msgfp, "rasterize> Final recursion level=%d, atom#%d...\n", recurlevel, natoms);
        if (expraster != NULL) {    /* i.e., if natoms>0 */
            type_raster(expraster->image, msgfp);       /* display completed raster */
        }
        fflush(msgfp);
    }
    /* flush msgfp buffer */
    /* --- set final raster buffer --- */
    if (1 && expraster != NULL) { /* have an expression */
        int type = expraster->type;     /* type of constructed image */
        if (type != FRACRASTER) /* leave \frac alone */
            expraster->type = IMAGERASTER;      /* set type to constructed image */
        if (istextmode)         /* but in text mode */
            expraster->type = blanksignal;      /* set type to avoid smash */
        expraster->size = fontsize;
    }
    /* set original input font size */
    /* --- restore flags/values to original saved values --- */
    isstring = wasstring;       /* string mode reset */
    isdisplaystyle = wasdisplaystyle;   /* displaystyle mode reset */
    fontnum = oldfontnum;       /* font family reset */
    fontsize = oldfontsize;     /* fontsize reset */
    displaysize = olddisplaysize;       /* \displaystyle size reset */
    shrinkfactor = oldshrinkfactor;     /* shrinkfactor reset */
    smashmargin = oldsmashmargin;       /* smashmargin reset */
    issmashdelta = oldissmashdelta;     /* issmashdelta reset */
    isexplicitsmash = oldisexplicitsmash;       /* isexplicitsmash reset */
    isscripted = oldisscripted; /* isscripted reset */
    workingparam = oldworkingparam;     /* working parameter reset */
    workingbox = oldworkingbox; /* working box reset */
    leftexpression = oldleftexpression; /* leftexpression reset */
    leftsymdef = oldleftsymdef; /* leftsymdef reset */
    unitlength = oldunitlength; /* unitlength reset */
    iunitlength = (int) (unitlength + 0.5);     /* iunitlength reset */
    recurlevel--;               /* unwind one recursion level */

    return expraster;
}

/* ==========================================================================
 * Function:    rastparen ( subexpr, size, basesp )
 * Purpose: parentheses handler, returns a subraster corresponding to
 *      parenthesized subexpression at font size
 * --------------------------------------------------------------------------
 * Arguments:   subexpr (I) char **  to first char of null-terminated
 *              string beginning with a LEFTBRACES
 *              to be rasterized
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding leading left{
 *              (unused, but passed for consistency)
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to subexpr,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o This "handler" isn't in the mathchardef symbol table,
 *      but is called directly from rasterize(), as necessary.
 *        o Though subexpr is returned unchanged, it's passed as char **
 *      for consistency with other handlers.  Ditto, basesp is unused
 *      but passed for consistency
 * ======================================================================= */
static subraster *rastparen(const char **subexpr, int size,
                            subraster * basesp) {
    const char *expression = *subexpr;  /* dereference subexpr to get char* */
    int explen = strlen(expression);    /* total #chars, including parens */
    int isescape = 0;           /* true if parens \escaped */
    int isrightdot = 0;         /* true if right paren is \right. */
    int isleftdot = 0;          /* true if left paren is \left. */
    char left[32], right[32];   /* parens enclosing expresion */
    char noparens[MAXSUBXSZ + 1];       /* get subexpr without parens */
    subraster *sp = NULL;       /* rasterize what's between ()'s */
    int isheight = 1;           /*true=full height, false=baseline */
    int height;                 /* height of rasterized noparens[] */
    int baseline;               /* and its baseline */
    int family = /*CMSYEX*/ CMEX10;     /* family for paren chars */
    subraster *lp = NULL, *rp = NULL;   /* left and right paren chars */
/* -------------------------------------------------------------------------
rasterize "interior" of expression, i.e., without enclosing parens
-------------------------------------------------------------------------- */
/* --- first see if enclosing parens are \escaped --- */
    if (isthischar(*expression, ESCAPE)) {      /* expression begins with \escape */
        isescape = 1;           /* so set flag accordingly */
    }
                                                                                                /* --- get expression *without* enclosing parens --- */ strcpy(noparens, expression);
                                                                                                /* get local copy of expression */
    noparens[explen - (1 + isescape)] = '\0';   /* null-terminate before right} */
    strsqueeze(noparens, (1 + isescape));       /* and then squeeze out left{ */
/* --- rasterize it --- */
    if ((sp = rasterize(noparens, size)) == NULL) {
        goto end_of_job;
    }
/* --- no need to add parentheses for unescaped { --- */
    if (!isescape && isthischar(*expression, "{")) {    /* don't add parentheses */
        goto end_of_job;        /* just return sp to caller */
    }
/* -------------------------------------------------------------------------
obtain paren characters to enclose noparens[] raster with
-------------------------------------------------------------------------- */
/* --- first get left and right parens from expression --- */
    memset(left, 0, 16);
    memset(right, 0, 16);       /* init parens with nulls */
    left[0] = *(expression + isescape); /* left{ is 1st or 2nd char */
    right[0] = *(expression + explen - 1);      /* right} is always last char */
    isleftdot = (isescape && isthischar(*left, "."));   /* true if \left. */
    isrightdot = (isescape && isthischar(*right, ".")); /* true if \right. */
/* --- need height of noparens[] raster as minimum parens height --- */
    height = (sp->image)->height;       /* height of noparens[] raster */
    baseline = sp->baseline;    /* baseline of noparens[] raster */
    if (!isheight)
        height = baseline + 1;  /* parens only enclose baseline up */
/* --- get best-fit parentheses characters --- */
    if (!isleftdot)             /* if not \left. */
        lp = get_delim(left, height + 1, family);       /* get left paren char */
    if (!isrightdot)            /* and if not \right. */
        rp = get_delim(right, height + 1, family);      /* get right paren char */
    if ((lp == NULL && !isleftdot)      /* check that we got left( */
        ||(rp == NULL && !isrightdot)) {        /* and right) if needed */
        delete_subraster(sp);   /* if failed, free subraster */
        if (lp != NULL)
            free((void *) lp);  /*free left-paren subraster envelope */
        if (rp != NULL)
            free((void *) rp);  /*and right-paren subraster envelope */
        sp = NULL;              /* signal error to caller */
        goto end_of_job;
    }

    /* and quit */
    /* -------------------------------------------------------------------------
       set paren baselines to center on noparens[] raster, and concat components
       -------------------------------------------------------------------------- */
    /* --- set baselines to center paren chars on raster --- */
    if (lp != NULL)             /* ignore for \left. */
        lp->baseline = baseline + ((lp->image)->height - height) / 2;
    if (rp != NULL)             /* ignore for \right. */
        rp->baseline = baseline + ((rp->image)->height - height) / 2;
/* --- concat lp||sp||rp to obtain final result --- */
    if (lp != NULL)             /* ignore \left. */
        sp = rastcat(lp, sp, 3);        /* concat lp||sp and free sp,lp */
    if (sp != NULL)             /* succeeded or ignored \left. */
        if (rp != NULL)         /* ignore \right. */
            sp = rastcat(sp, rp, 3);    /* concat sp||rp and free sp,rp */

  end_of_job:
    return sp;
}

/* ==========================================================================
 * Function:    rastlimits ( expression, size, basesp )
 * Purpose: \limits, \nolimts, _ and ^ handler,
 *      dispatches call to rastscripts() or to rastdispmath()
 *      as necessary, to handle sub/superscripts following symbol
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char **  to first char of null-terminated
 *              LaTeX expression (unused/unchanged)
 *      size (I)    int containing base font size (not used,
 *              just stored in subraster)
 *      basesp (I)  subraster *  to current character (or
 *              subexpression) immediately preceding script
 *              indicator
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster returned by rastscripts()
 *              or rastdispmath(), or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *rastlimits(const char **expression, int size,
                             subraster * basesp) {
    subraster *scriptsp = basesp;       /* and this will become the result */
    subraster *dummybase = basesp;      /* for {}_i construct a dummy base */
    int isdisplay = -1;         /* set 1 for displaystyle, else 0 */
    int oldsmashmargin = smashmargin;   /* save original smashmargin */
/* --- to check for \limits or \nolimits preceding scripts --- */
    const char *exprptr = *expression;
    char limtoken[255];         /*check for \limits */
    int toklen = 0;             /* strlen(limtoken) */
    mathchardef *tokdef;        /* mathchardef struct for limtoken */
    int class = (leftsymdef == NULL ? NOVALUE : leftsymdef->class);     /*base sym class */
/* -------------------------------------------------------------------------
determine whether or not to use displaymath
-------------------------------------------------------------------------- */
     scriptlevel++;             /* first, increment subscript level */
    *limtoken = '\0';           /* no token yet */
     isscripted = 0;            /* signal term not (text) scripted */
    if (msgfp != NULL && msglevel >= 999) {
        fprintf(msgfp, "rastlimits> scriptlevel#%d exprptr=%.48s\n",
                scriptlevel, (exprptr == NULL ? "null" : exprptr));
        fflush(msgfp);
    }
    if (isstring)
         goto end_of_job;       /* no scripts for ascii string */
/* --- check for \limits or \nolimits --- */
    skipwhite(exprptr);         /* skip white space before \limits */
    if (!isempty(exprptr))      /* non-empty expression supplied */
        exprptr = texchar(exprptr, limtoken);   /* retrieve next token */
    if (*limtoken != '\0')      /* have token */
        if ((toklen = strlen(limtoken)) >= 3)   /* which may be \[no]limits */
            if (memcmp("\\limits", limtoken, toklen) == 0       /* may be \limits */
                || memcmp("\\nolimits", limtoken, toklen) == 0) /* or may be \nolimits */
                if ((tokdef = get_symdef(limtoken))     /* look up token to be sure */
                    !=NULL) {   /* found token in table */
                    if (strcmp("\\limits", tokdef->symbol) == 0)        /* found \limits */
                        isdisplay = 1;  /* so explicitly set displaymath */
                    else /* wasn't \limits */ if (strcmp("\\nolimits", tokdef->symbol) == 0)    /* found \nolimits */
                        isdisplay = 0;
                }
    /* so explicitly reset displaymath */
    /* --- see if we found \[no]limits --- */
    if (isdisplay != -1) {      /* explicit directive found */
        *expression = exprptr;  /* so bump expression past it */
    } else {                    /* noexplicit directive */
        isdisplay = 0;          /* init displaymath flag off */
        if (isdisplaystyle) {   /* we're in displaystyle math mode */
            if (isdisplaystyle >= 5) {  /* and mode irrevocably forced true */
                if (class != OPENING && class != CLOSING) {     /*don't force ('s and )'s */
                    isdisplay = 1;
                }
            } else if (isdisplaystyle >= 2) {   /*or mode forced conditionally true */
                if (class != VARIABLE && class != ORDINARY      /*don't force characters */
                    && class != OPENING && class != CLOSING     /*don't force ('s and )'s */
                    && class != BINARYOP        /* don't force binary operators */
                    && class != NOVALUE)
                    /* finally, don't force "images" */
                    isdisplay = 1;
            } else if (class == DISPOPER) {     /* it's a displaystyle operator */
                isdisplay = 1;
            }
        }
    }
/* -------------------------------------------------------------------------
dispatch call to create sub/superscripts
-------------------------------------------------------------------------- */
    if (isdisplay)              /* scripts above/below base symbol */
        scriptsp = rastdispmath(expression, size, basesp);      /* everything all done */
    else {                      /* scripts alongside base symbol */
        if (dummybase == NULL)  /* no base symbol preceding scripts */
            dummybase = rasterize("\\rule0{10}", size); /*guess a typical base symbol */
        issmashokay = 1;        /*haven't found a no-smash char yet */
        if ((scriptsp = rastscripts(expression, size, dummybase)) == NULL)      /*no scripts */
            scriptsp = basesp;  /* so just return unscripted symbol */
        else {                  /* symbols followed by scripts */
            isscripted = 1;     /*signal current term text-scripted */
            if (basesp != NULL) {       /* have base symbol *//*if(0)smashmargin = 0; *//*don't smash script (doesn't work) */
                /*scriptsp = rastcat(basesp,scriptsp,2); *//*concat scripts to base sym */
                /* --- smash (or just concat) script raster against base symbol --- */
                if (!issmashokay)       /* don't smash leading - */
                    if (!isexplicitsmash)
                        scriptsp->type = blanksignal;   /*don't smash */
                scriptsp = rastcat(basesp, scriptsp, 3);        /*concat scripts to base sym */
                if (1)
                    scriptsp->type = IMAGERASTER;       /* flip type of composite object */
                /* --- smash (or just concat) scripted term to stuff to its left --- */
                issmashokay = 1;        /* okay to smash base expression */
                if (0 && smashcheck > 1) {
                    /* smashcheck=2 to check base */
                    /* note -- we _don't_ have base expression available to check */
                    issmashokay = rastsmashcheck(*expression);  /*check if okay to smash */
                }
                if (!issmashokay) {     /* don't smash leading - */
                    if (!isexplicitsmash) {
                        scriptsp->type = blanksignal;   /*don't smash */
                    }
                }
                scriptsp->size = size;
            }
        }
    }                           /* and set font size */
  end_of_job:
    smashmargin = oldsmashmargin;       /* reset original smashmargin */
    if (dummybase != basesp)
        delete_subraster(dummybase);    /*free work area */
    if (msgfp != NULL && msglevel >= 99) {
        fprintf(msgfp, "rastlimits> scriptlevel#%d returning %s\n",
                scriptlevel, (scriptsp == NULL ? "null" : "..."));
        if (scriptsp != NULL)   /* have a constructed raster */
            type_raster(scriptsp->image, msgfp);        /*display constructed raster */
        fflush(msgfp);
    }
    scriptlevel--;              /*lastly, decrement subscript level */
    return scriptsp;
}

/* ==========================================================================
 * Function:    rastscripts ( expression, size, basesp )
 * Purpose: super/subscript handler, returns subraster for the leading
 *      scripts in expression, whose base symbol is at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string beginning with a super/subscript,
 *              and returning ptr immediately following
 *              last script character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding leading script
 *              (scripts will be placed relative to base)
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to scripts,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o This "handler" isn't in the mathchardef symbol table,
 *      but is called directly from rasterize(), as necessary.
 * ======================================================================= */
static subraster *rastscripts(const char **expression, int size,
                              subraster * basesp) {
    char subscript[512];
    char supscript[512];        /* scripts parsed from expression */
    subraster *subsp = NULL, *supsp = NULL;     /* rasterize scripts */
    subraster *sp = NULL;       /* super- over subscript subraster */
    raster *rp = NULL;          /* image raster embedded in sp */
    int height = 0, width = 0, baseline = 0;    /* height,width,baseline of sp */
    int subht = 0, subwidth = 0 /* not used --, subln = 0 */ ;  /* height,width,baseline of sub */
    int supht = 0, supwidth = 0 /* not used --, supln = 0 */ ;  /* height,width,baseline of sup */
    int baseht = 0, baseln = 0; /* height,baseline of base */
    int bdescend = 0, sdescend = 0;     /* descender of base, subscript */
    int issub = 0, issup = 0, isboth = 0;       /* true if we have sub,sup,both */
                                        /* not used -- int isbase = 0 */ ;
                                        /* true if we have base symbol */
    int szval = min2(max2(size, 0), LARGESTSIZE),       /* 0...LARGESTSIZE */
        vbetween = 2,           /* vertical space between scripts */
        vabove = szval + 1,     /*sup's top/bot above base's top/bot */
        vbelow = szval + 1,     /*sub's top/bot below base's top/bot */
        vbottom = szval + 1;    /*sup's bot above (sub's below) bsln */
    /*int   istweak = 1; *//* true to tweak script positioning */
    int pixsz = 1;              /*default #bits per pixel, 1=bitmap */
/* -------------------------------------------------------------------------
Obtain subscript and/or superscript expressions, and rasterize them/it
-------------------------------------------------------------------------- */
/* --- parse for sub,superscript(s), and bump expression past it(them) --- */
    if (expression == NULL)
        goto end_of_job;        /* no *ptr given */
    if (*expression == NULL)
        goto end_of_job;        /* no expression given */
    if (*(*expression) == '\0')
        goto end_of_job;        /* nothing in expression */
    *expression = texscripts(*expression, subscript, supscript, 3);
/* --- rasterize scripts --- */
    if (*subscript != '\0')     /* have a subscript */
         subsp = rasterize(subscript, size - 1);        /* so rasterize it at size-1 */
    if (*supscript != '\0')     /* have a superscript */
         supsp = rasterize(supscript, size - 1);        /* so rasterize it at size-1 */
/* --- set flags for convenience --- */
     issub = (subsp != (subraster *) NULL);     /* true if we have subscript */
     issup = (supsp != (subraster *) NULL);     /* true if we have superscript */
     isboth = (issub && issup); /* true if we have both */
    if (!issub && !issup)
         goto end_of_job;       /* quit if we have neither */
/* --- check for leading no-smash chars (if enabled) --- */
     issmashokay = 0;           /* default, don't smash scripts */
    if (smashcheck > 0) {       /* smash checking wanted */
        issmashokay = 1;        /*haven't found a no-smash char yet */
        if (issub)              /* got a subscript */
            issmashokay = rastsmashcheck(subscript);    /* check if okay to smash */
        if (issmashokay)        /* clean sub, so check sup */
            if (issup)          /* got a superscript */
                issmashokay = rastsmashcheck(supscript);        /* check if okay to smash */
    }

    /* --- end-of-if(smashcheck>0) --- *//* -------------------------------------------------------------------------
       get height, width, baseline of scripts,  and height, baseline of base symbol
                                                                                                                                                           -------------------------------------------------------------------------- *//* --- get height and width of components --- */ if (issub) {
                                                                                                                                                        /* we have a subscript */
        subht = (subsp->image)->height; /* so get its height */
        subwidth = (subsp->image)->width;       /* and width */
        /* not used --subln = subsp->baseline */ ;
    }                           /* and baseline */
    if (issup) {                /* we have a superscript */
        supht = (supsp->image)->height; /* so get its height */
        supwidth = (supsp->image)->width;       /* and width */
        /* not used --supln = supsp->baseline */ ;
    }
    /* and baseline */
    /* --- get height and baseline of base, and descender of base and sub --- */
    if (basesp == NULL) {       /* no base symbol for scripts */
        basesp = leftexpression;        /* try using left side thus far */
    }
    if (basesp != NULL) {       /* we have base symbol for scripts */
        baseht = (basesp->image)->height;       /* height of base symbol */
        baseln = basesp->baseline;      /* and its baseline */
        bdescend = baseht - (baseln + 1);       /* and base symbol descender */
        sdescend = bdescend + vbelow;   /*sub must descend by at least this */
        /* not used -- if (baseht > 0)
           isbase = 1; */
    }
    /* -------------------------------------------------------------------------
       determine width of constructed raster
       -------------------------------------------------------------------------- */
    width = max2(subwidth, supwidth);   /*widest component is overall width */
/* -------------------------------------------------------------------------
determine height and baseline of constructed raster
-------------------------------------------------------------------------- */
/* --- both super/subscript --- */
    if (isboth) {               /*we have subscript and superscript */
        height = max2(subht + vbetween + supht, /* script heights + space bewteen */
                      vbelow + baseht + vabove);        /*sub below base bot, sup above top */
        baseline = baseln + (height - baseht) / 2;
    }
    /*center scripts on base symbol */
    /* --- superscript only --- */
    if (!issub) {               /* we only have a superscript */
        height = max3(baseln + 1 + vabove,      /* sup's top above base symbol top */
                      supht + vbottom,  /* sup's bot above baseln */
                      supht + vabove - bdescend);       /* sup's bot above base symbol bot */
        baseline = height - 1;
    }
    /*sup's baseline at bottom of raster */
    /* --- subscript only --- */
    if (!issup) {               /* we only have a subscript */
        if (subht > sdescend) { /*sub can descend below base bot... */
            height = subht;     /* ...without extra space on top */
            baseline = height - (sdescend + 1); /* sub's bot below base symbol bot */
            baseline = min2(baseline, max2(baseln - vbelow, 0));
        } /*top below base top */
        else {                  /* sub's top will be below baseln */
            height = sdescend + 1;      /* sub's bot below base symbol bot */
            baseline = 0;
        }
    }

    /* sub's baseline at top of raster */
    /* -------------------------------------------------------------------------
       construct raster with superscript over subscript
       -------------------------------------------------------------------------- */
    /* --- allocate subraster containing constructed raster --- */
    if ((sp = new_subraster(width, height, pixsz)) == NULL) {
        goto end_of_job;
    }
/* --- initialize subraster parameters --- */
    sp->type = IMAGERASTER;     /* set type as constructed image */
    sp->size = size;            /* set given size */
    sp->baseline = baseline;    /* composite scripts baseline */
    rp = sp->image;             /* raster embedded in subraster */
/* --- place super/subscripts in new raster --- */
    if (issup)                  /* we have a superscript */
        rastput(rp, supsp->image, 0, 0, 1);     /* it goes in upper-left corner */
    if (issub)                  /* we have a subscript */
        rastput(rp, subsp->image, height - subht, 0, 1);        /*in lower-left corner */
/* -------------------------------------------------------------------------
free unneeded component subrasters and return final result to caller
-------------------------------------------------------------------------- */
  end_of_job:
    if (issub) {
        delete_subraster(subsp);
    }
    if (issup) {
        delete_subraster(supsp);
    }
    return sp;
}

/* ==========================================================================
 * Function:    rastdispmath ( expression, size, sp )
 * Purpose: displaymath handler, returns sp along with
 *      its immediately following super/subscripts
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following sp to be
 *              rasterized along with its super/subscripts,
 *              and returning ptr immediately following last
 *              character processed.
 *      size (I)    int containing 0-7 default font size
 *      sp (I)      subraster *  to display math operator
 *              to which super/subscripts will be added
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to sp
 *              plus its scripts, or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o sp returned unchanged if no super/subscript(s) follow it.
 * ======================================================================= */
static subraster *rastdispmath(const char **expression, int size,
                               subraster * sp) {
    char subscript[512];
    char supscript[512];        /* scripts parsed from expression */
    int issub = 0, issup = 0;   /* true if we have sub,sup */
    subraster *subsp = NULL, *supsp = NULL;     /* rasterize scripts */
    int vspace = 1;             /* vertical space between scripts */
/* -------------------------------------------------------------------------
Obtain subscript and/or superscript expressions, and rasterize them/it
-------------------------------------------------------------------------- */
/* --- parse for sub,superscript(s), and bump expression past it(them) --- */
    if (expression == NULL) {
        goto end_of_job;        /* no *ptr given */
    }
    if (*expression == NULL) {
        goto end_of_job;        /* no expression given */
    }
    if (*(*expression) == '\0') {
        goto end_of_job;        /* nothing in expression */
    }
    *expression = texscripts(*expression, subscript, supscript, 3);
/* --- rasterize scripts --- */
    if (*subscript != '\0') {   /* have a subscript */
        subsp = rasterize(subscript, size - 1); /* so rasterize it at size-1 */
    }
    if (*supscript != '\0') {   /* have a superscript */
        supsp = rasterize(supscript, size - 1); /* so rasterize it at size-1 */
    }
/* --- set flags for convenience --- */
    issub = (subsp != NULL);    /* true if we have subscript */
    issup = (supsp != NULL);    /* true if we have superscript */
    if (!issub && !issup) {
        goto end_of_job;        /*return operator alone if neither */
    }
/* -------------------------------------------------------------------------
stack operator and its script(s)
-------------------------------------------------------------------------- */
/* --- stack superscript atop operator --- */
    if (issup) {                /* we have a superscript */
        if (sp == NULL) {       /* but no base expression */
            sp = supsp;         /* so just use superscript */
        } else if ((sp = rastack(sp, supsp, 1, vspace, 1, 3)) == NULL) {
            /* have base and superscript */
            goto end_of_job;
        }
    }
    /* and quit if failed */
    /* --- stack operator+superscript atop subscript --- */
    if (issub) {                /* we have a subscript */
        if (sp == NULL) {       /* but no base expression */
            sp = subsp;         /* so just use subscript */
        } else if ((sp = rastack(subsp, sp, 2, vspace, 1, 3)) == NULL) {
            goto end_of_job;
        }
    }
    sp->type = IMAGERASTER;     /* flip type of composite object */
    sp->size = size;            /* and set font size */
/* -------------------------------------------------------------------------
free unneeded component subrasters and return final result to caller
-------------------------------------------------------------------------- */
  end_of_job:
    return sp;
}

/* ==========================================================================
 * Function:    rastleft ( expression, size, basesp, ildelim, arg2, arg3 )
 * Purpose: \left...\right handler, returns a subraster corresponding to
 *      delimited subexpression at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char **  to first char of null-terminated
 *              string beginning with a \left
 *              to be rasterized
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding leading left{
 *              (unused, but passed for consistency)
 *      ildelim (I) int containing ldelims[] index of
 *              left delimiter
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to subexpr,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
subraster *rastleft(const char **expression, int size, subraster * basesp,
                    int ildelim, int arg2, int arg3) {
    subraster *sp = NULL;       /*rasterize between \left...\right */
    subraster *lp = NULL, *rp = NULL;   /* left and right delim chars */
    int family = CMSYEX;        /* get_delim() family */
    int height = 0, rheight = 0;        /* subexpr, right delim height */
    int margin = size + 1;      /* delim height margin over subexpr */
    int opmargin = 5;           /* extra margin for \int,\sum,\etc */
    char /* *texleft(), */ subexpr[MAXSUBXSZ + 1];      /*chars between \left...\right */
    char ldelim[256] = ".";
    char rdelim[256] = ".";     /* delims following \left,\right */
    const char *pleft;
    const char *pright;         /*locate \right matching our \left */
    int isleftdot = 0, isrightdot = 0;  /* true if \left. or \right. */
    /* not used --int isleftscript = 0; */
    int isrightscript = 0;      /* true if delims are scripted */
    int sublen = 0;             /* strlen(subexpr) */
    int idelim = 0;             /* 1=left,2=right */
    /* int  gotldelim = 0; *//* true if ildelim given by caller */
    int wasdisplaystyle = isdisplaystyle;       /* save current displaystyle */
    int istextleft = 0, istextright = 0;        /* true for non-displaystyle delims */
/* --- recognized delimiters --- */
    static char left[16] = "\\left";
    static char right[16] = "\\right";  /* tex delimiters */
    static const char *ldelims[] = {
        "unused", ".",          /* 1   for \left., \right. */
            "(", ")",           /* 2,3 for \left(, \right) */
            "\\{", "\\}",       /* 4,5 for \left\{, \right\} */
            "[", "]",           /* 6,7 for \left[, \right] */
            "<", ">",           /* 8,9 for \left<, \right> */
            "|", "\\|",         /* 10,11 for \left,\right |,\| */
    NULL};
/* --- recognized operator delimiters --- */
    static const char *opdelims[] = {   /* operator delims from cmex10 */
    "int", "sum", "prod", "cup", "cap", "dot", "plus", "times",
            "wedge", "vee", NULL};
/* --- delimiter xlation --- */
    static const char *xfrom[] =        /* xlate any delim suffix... */
    {
        "\\|",                  /* \| */
            "\\{",              /* \{ */
            "\\}",              /* \} */
            "\\lbrace",         /* \lbrace */
            "\\rbrace",         /* \rbrace */
            "\\langle",         /* \langle */
            "\\rangle",         /* \rangle */
    NULL};
    static const char *xto[] =  /* ...to this instead */
    {
        "=",                    /* \| to = */
            "{",                /* \{ to { */
            "}",                /* \} to } */
            "{",                /* \lbrace to { */
            "}",                /* \rbrace to } */
            "<",                /* \langle to < */
            ">",                /* \rangle to > */
    NULL};
/* --- non-displaystyle delimiters --- */
    static const char *textdelims[] =   /* these delims _aren't_ display */
    {
        "|", "=", "(", ")", "[", "]", "<", ">", "{", "}", "dbl",        /* \lbrackdbl and \rbrackdbl */
    NULL};
/* -------------------------------------------------------------------------
initialization
-------------------------------------------------------------------------- */
/* --- check args --- */
    if (*(*expression) == '\0') {
        goto end_of_job;        /* nothing after \left */
    }
/* --- determine left delimiter, and set default \right. delimiter --- */
    if (ildelim != NOVALUE && ildelim >= 1) {   /* called with explicit left delim */
        strcpy(ldelim, ldelims[ildelim]);       /* so just get a local copy */
        /* gotldelim = 1; */
    } else {                    /* trapped \left without delim */
        skipwhite(*expression); /* interpret \left ( as \left( */
        if (*(*expression) == '\0') {   /* end-of-string after \left */
            goto end_of_job;    /* so return NULL */
        }
        *expression = texchar(*expression, ldelim);     /*pull delim from expression */
        if (*expression == NULL || *ldelim == '\0') {   /* probably invalid end-of-string */
            goto end_of_job;
        }
    }                           /* no delimiter */
    strcpy(rdelim, ".");        /* init default \right. delim */
/* -------------------------------------------------------------------------
locate \right balancing our opening \left
-------------------------------------------------------------------------- */
/* --- first \right following \left --- */
    if ((pright = strtexchr(*expression, right))        /* look for \right after \left */
        !=NULL) {               /* found it */
        /* --- find matching \right by pushing past any nested \left's --- */
        pleft = *expression;    /* start after first \left( */
        while (1) {             /*break when matching \right found */
            /* -- locate next nested \left if there is one --- */
            if ((pleft = strtexchr(pleft, left))        /* find next \left */
                == NULL)
                break;          /*no more, so matching \right found */
            pleft += strlen(left);      /* push ptr past \left token */
            if (pleft >= pright)
                break;          /* not nested if \left after \right */
            /* --- have nested \left, so push forward to next \right --- */
            if ((pright = strtexchr(pright + strlen(right), right))     /* find next \right */
                == NULL)
                break;          /* ran out of \right's */
        }                       /* --- end-of-while(1) --- */
    }
    /* -------------------------------------------------------------------------
       push past \left(_a^b sub/superscripts, if present
       -------------------------------------------------------------------------- */
    pleft = *expression;        /*reset pleft after opening \left( */
    if ((lp = rastlimits(expression, size, lp)) != NULL) {
        /* found actual _a^b scripts, too */
        delete_subraster(lp);   /* but we don't need them */
        lp = NULL;
    }

    /* reset pointer, too */
    /* -------------------------------------------------------------------------
       get \right delimiter and subexpression between \left...\right, xlate delims
       -------------------------------------------------------------------------- */
    /* --- get delimiter following \right --- */
    if (pright == NULL) {       /* assume \right. at end of exprssn */
        strcpy(rdelim, ".");    /* set default \right. */
        sublen = strlen(*expression);   /* use entire remaining expression */
        memcpy(subexpr, *expression, sublen);   /* copy all remaining chars */
        *expression += sublen;
    } else {                    /* have explicit matching \right */
        sublen = (int) (pright - (*expression));        /* #chars between \left...\right */
        memcpy(subexpr, *expression, sublen);   /* copy chars preceding \right */
        *expression = pright + strlen(right);   /* push expression past \right */
        skipwhite(*expression); /* interpret \right ) as \right) */
        *expression = texchar(*expression, rdelim);     /*pull delim from expression */
        if (*rdelim == '\0') {
            strcpy(rdelim, ".");
        }
    }                           /* \right. if no rdelim */
/* --- get subexpression between \left...\right --- */
    if (sublen < 1) {
        goto end_of_job;        /* nothing between delimiters */
    }
    subexpr[sublen] = '\0';     /* and null-terminate it */
/* --- adjust margin for expressions containing \middle's --- */
    if (strtexchr(subexpr, "\\middle") != NULL) {       /* have enclosed \middle's */
        margin = 1;             /* so don't "overwhelm" them */
    }
/* --- check for operator delimiter --- */
    for (idelim = 0; opdelims[idelim] != NULL; idelim++) {
        if (strstr(ldelim, opdelims[idelim]) != NULL) { /* found operator */
            margin += opmargin; /* extra height for operator */
            if (*ldelim == '\\') {      /* have leading escape */
                strsqueeze(ldelim, 1);
            }
            break;
        }
    }
    /* no need to check rest of table */
    /* --- xlate delimiters and check for textstyle --- */
    for (idelim = 1; idelim <= 2; idelim++) {   /* 1=left, 2=right */
        char *lrdelim = (idelim == 1 ? ldelim : rdelim);        /* ldelim or rdelim */
        int ix;
        const char *xdelim;     /* xfrom[] and xto[] index, delim */
        for (ix = 0; (xdelim = xfrom[ix]) != NULL; ix++) {
            if (strcmp(lrdelim, xdelim) == 0) { /* found delim to xlate */
                strcpy(lrdelim, xto[ix]);       /* replace with corresponding xto[] */
                break;
            }
        }
        for (ix = 0; (xdelim = textdelims[ix]) != NULL; ix++) {
            if (strstr(lrdelim, xdelim) != 0) { /* found textstyle delim */
                if (idelim == 1) {      /* if it's the \left one */
                    istextleft = 1;     /* set left textstyle flag */
                } else {
                    istextright = 1;    /* else set right textstyle flag */
                }
                break;
            }
        }
    }
/* --- debugging --- */
    if (msgfp != NULL && msglevel >= 99)
        fprintf(msgfp,
                "rastleft> left=\"%s\" right=\"%s\" subexpr=\"%s\"\n",
                ldelim, rdelim, subexpr);
/* -------------------------------------------------------------------------
rasterize subexpression
-------------------------------------------------------------------------- */
/* --- rasterize subexpression --- */
    if ((sp = rasterize(subexpr, size)) == NULL) {
        goto end_of_job;        /* quit if failed */
    }
    height = sp->image->height; /* height of subexpr raster */
    rheight = height + margin;  /*default rheight as subexpr height */
/* -------------------------------------------------------------------------
rasterize delimiters, reset baselines, and add  sub/superscripts if present
-------------------------------------------------------------------------- */
/* --- check for dot delimiter --- */
    isleftdot = (strchr(ldelim, '.') != NULL);  /* true if \left. */
    isrightdot = (strchr(rdelim, '.') != NULL); /* true if \right. */
/* --- get rasters for best-fit delim characters, add sub/superscripts --- */
    isdisplaystyle = (istextleft ? 0 : 9);      /* force \displaystyle */
    if (!isleftdot) {           /* if not \left. *//* --- first get requested \left delimiter --- */
        lp = get_delim(ldelim, rheight, family);        /* get \left delim char */
        /* --- reset lp delim baseline to center delim on subexpr raster --- */
        if (lp != NULL) {       /* if get_delim() succeeded */
            int lheight = (lp->image)->height;  /* actual height of left delim */
            lp->baseline = sp->baseline + (lheight - height) / 2;
            if (lheight > rheight)      /* got bigger delim than requested */
                rheight = lheight - 1;
        }
        /* make sure right delim matches */
        /* --- then add on any sub/superscripts attached to \left( --- */
        lp = rastlimits(&pleft, size, lp);      /*\left(_a^b and push pleft past b */
        /* not used --isleftscript = isscripted; */
    }
    isdisplaystyle = (istextright ? 0 : 9);     /* force \displaystyle */
    if (!isrightdot) {
        /* and if not \right. */
        /* --- first get requested \right delimiter --- */
        rp = get_delim(rdelim, rheight, family);        /* get \right delim char */
        /* --- reset rp delim baseline to center delim on subexpr raster --- */
        if (rp != NULL) {
            rp->baseline = sp->baseline + (rp->image->height - height) / 2;
        }
        /* --- then add on any sub/superscripts attached to \right) --- */
        rp = rastlimits(expression, size, rp);  /*\right)_c^d, expression past d */
        isrightscript = isscripted;
    }
    isdisplaystyle = wasdisplaystyle;   /* original \displystyle default */
/* --- check that we got delimiters --- */
    if (0) {
        if ((lp == NULL && !isleftdot)  /* check that we got left( */
            ||(rp == NULL && !isrightdot)) {    /* and right) if needed */
            if (lp != NULL)
                free((void *) lp);      /* free \left-delim subraster */
            if (rp != NULL)
                free((void *) rp);      /* and \right-delim subraster */
            if (0) {
                delete_subraster(sp);   /* if failed, free subraster */
                sp = (subraster *) NULL;
            }                   /* signal error to caller */
            goto end_of_job;
        }
    }

    /* -------------------------------------------------------------------------
       concat  lp || sp || rp  components
       -------------------------------------------------------------------------- */
    /* --- concat lp||sp||rp to obtain final result --- */
    if (lp != NULL)             /* ignore \left. */
        sp = rastcat(lp, sp, 3);        /* concat lp||sp and free sp,lp */
    if (sp != NULL)             /* succeeded or ignored \left. */
        if (rp != NULL)         /* ignore \right. */
            sp = rastcat(sp, rp, 3);    /* concat sp||rp and free sp,rp */
/* --- back to caller --- */
  end_of_job:
    isdelimscript = isrightscript;      /* signal if right delim scripted */
    return sp;
}

#if 0
/* ==========================================================================
 * Function:    rastright ( expression, size, basesp, ildelim, arg2, arg3 )
 * Purpose: ...\right handler, intercepts an unexpected/unbalanced \right
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char **  to first char of null-terminated
 *              string beginning with a \right
 *              to be rasterized
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding leading left{
 *              (unused, but passed for consistency)
 *      ildelim (I) int containing rdelims[] index of
 *              right delimiter
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to subexpr,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *rastright(const char **expression, int size,
                            subraster * basesp, int ildelim, int arg2,
                            int arg3) {
    subraster *sp = NULL;       /*rasterize \right subexpr's */
    if (sp != NULL) {           /* returning entire expression */
        isreplaceleft = 1;      /* set flag to replace left half */
    }
    return (sp);
}
#endif


/* ==========================================================================
 * Function:    rastmiddle ( expression, size, basesp,  arg1, arg2, arg3 )
 * Purpose: \middle handler, returns subraster corresponding to
 *      entire expression with \middle delimiter(s) sized to fit.
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \middle to be
 *              rasterized, and returning ptr immediately
 *              to terminating null.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \middle
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to expression,
 *              or NULL for any parsing error
 *              (expression ptr unchanged if error occurs)
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *rastmiddle(const char **expression, int size,
                             subraster * basesp, int arg1, int arg2,
                             int arg3) {
    subraster *sp = NULL, *subsp[32];   /*rasterize \middle subexpr's */
    const char *exprptr = *expression;  /* local copy of ptr to expression */
    char delim[32][132];        /* delimiters following \middle's */
    char subexpr[MAXSUBXSZ + 1];
    char *subptr = NULL;        /*subexpression between \middle's */
    int height = 0, habove = 0, hbelow = 0;     /* height, above & below baseline */
    int idelim, ndelims = 0;    /* \middle count (max 32) */
    int family = CMSYEX;        /* delims from CMSY10 or CMEX10 */
/* -------------------------------------------------------------------------
initialization
-------------------------------------------------------------------------- */
     subsp[0] = leftexpression; /* expressn preceding 1st \middle */
     subsp[1] = NULL;           /* set first null */
/* -------------------------------------------------------------------------
accumulate subrasters between consecutive \middle\delim...\middle\delim...'s
-------------------------------------------------------------------------- */
    while (ndelims < 30) {      /* max of 31 \middle's */
        /* --- maintain max height above,below baseline --- */
        if (subsp[ndelims] != NULL) {   /*exprssn preceding current \middle */
            int baseline = (subsp[ndelims])->baseline;  /* #rows above baseline */
             height = ((subsp[ndelims])->image)->height;        /* tot #rows (height) */
             habove = max2(habove, baseline);   /* max #rows above baseline */
             hbelow = max2(hbelow, height - baseline);
        }
                                                                                                        /* max #rows below baseline *//* --- get delimter after \middle --- */ skipwhite(exprptr);
                                                                                                        /*skip space betwn \middle & \delim */
        exprptr = texchar(exprptr, delim[ndelims]);     /* \delim after \middle */
        if (*(delim[ndelims]) == '\0') {        /* \middle at end-of-expression */
            break;              /* ignore it and consider job done */
        }
        ndelims++;              /* count another \middle\delim */
        /* --- get subexpression between \delim and next \middle --- */
        subsp[ndelims] = NULL;  /* no subexpresion yet */
        if (*exprptr == '\0') { /* end-of-expression after \delim */
            break;              /* so we have all subexpressions */
        }
        if ((subptr = strtexchr(exprptr, "\\middle")) == NULL) {
            /* no more \middle's */
            strncpy(subexpr, exprptr, MAXSUBXSZ);       /*get entire remaining expression */
            subexpr[MAXSUBXSZ] = '\0';  /* make sure it's null-terminated */
            exprptr += strlen(exprptr);
        } else {
            /* have another \middle */
            int sublen = (int) (subptr - exprptr);      /* #chars between \delim...\middle */
            memcpy(subexpr, exprptr, min2(sublen, MAXSUBXSZ));  /* get subexpression */
            subexpr[min2(sublen, MAXSUBXSZ)] = '\0';    /* and null-terminate it */
            exprptr += (sublen + strlen("\\middle"));
        }
        /* --- rasterize subexpression --- */
        subsp[ndelims] = rasterize(subexpr, size);      /* rasterize subexpresion */
    }
/* -------------------------------------------------------------------------
construct \middle\delim's and concatanate them between subexpressions
-------------------------------------------------------------------------- */
    if (ndelims < 1             /* no delims */
        || (height = habove + hbelow) < 1)      /* or no subexpressions? */
        goto end_of_job;        /* just flush \middle directive */
    for (idelim = 0; idelim <= ndelims; idelim++) {
        /* --- first add on subexpression preceding delim --- */
        if (subsp[idelim] != NULL) {    /* have subexpr preceding delim */
            if (sp == NULL) {   /* this is first piece */
                sp = subsp[idelim];     /* so just use it */
                if (idelim == 0)
                    sp = subrastcpy(sp);
            } else {
                sp = rastcat(sp, subsp[idelim], (idelim > 0 ? 3 : 1));
            }
        }
        /* or concat it */
        /* --- now construct delimiter --- */
        if (*(delim[idelim]) != '\0') { /* have delimter */
            subraster *delimsp = get_delim(delim[idelim], height, family);
            if (delimsp != NULL) {      /* rasterized delim */
                delimsp->baseline = habove;     /* set baseline */
                if (sp == NULL) {       /* this is first piece */
                    sp = delimsp;       /* so just use it */
                } else {
                    sp = rastcat(sp, delimsp, 3);
                }
            }
        }                       /*or concat to existing pieces */
    }                           /* --- end-of-for(idelim) --- */
/* --- back to caller --- */
  end_of_job:
    if (0) {
        for (idelim = 1; idelim <= ndelims; idelim++) { /* free subsp[]'s (not 0) */
            if (subsp[idelim] != NULL) {        /* have allocated subraster */
                delete_subraster(subsp[idelim]);        /* so free it */
            }
        }
    }
    if (sp != NULL) {           /* returning entire expression */
        int newht = (sp->image)->height;        /* height of returned subraster */
        sp->baseline = min2(newht - 1, newht / 2 + 5);  /* guess new baseline */
        isreplaceleft = 1;      /* set flag to replace left half */
        *expression += strlen(*expression);
    }                           /* and push to terminating null */
    return (sp);
}                               /* --- end-of-function rastmiddle() --- */

/* ==========================================================================
 * Function:    rastflags ( expression, size, basesp,  flag, value, arg3 )
 * Purpose: sets an internal flag, e.g., for \rm, or sets an internal
 *      value, e.g., for \unitlength=<value>, and returns NULL
 *      so nothing is displayed
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char **  to first char of null-terminated
 *              LaTeX expression (unused/unchanged)
 *      size (I)    int containing base font size (not used,
 *              just stored in subraster)
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding "flags" directive
 *              (unused but passed for consistency)
 *      flag (I)    int containing #define'd symbol specifying
 *              internal flag to be set
 *      value (I)   int containing new value of flag
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) NULL so nothing is displayed
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *rastflags(const char **expression, int size,
                            subraster * basesp, int flag, int value,
                            int arg3) {
    char valuearg[1024] = "NOVALUE";    /* value from expression, if needed */
    int argvalue = NOVALUE;     /* atoi(valuearg) */
    int isdelta = 0;            /* true if + or - precedes valuearg */
    int valuelen = 0;           /* strlen(valuearg) */
    static int displaystylelevel = -99; /* \displaystyle set at recurlevel */
/* -------------------------------------------------------------------------
set flag or value
-------------------------------------------------------------------------- */
    switch (flag) {
    default:
        break;                  /* unrecognized flag */
        case ISFONTFAM:if (isthischar((*(*expression)), WHITEMATH)) {   /* \rm followed by white */
            (*expression)++;    /* skip leading ~ after \rm */
        }
        fontnum = value;        /* set font family */
        break;
    case ISSTRING:
        isstring = value;
        break;                  /* set string/image mode */
    case ISDISPLAYSTYLE:       /* set \displaystyle mode */
        displaystylelevel = recurlevel; /* \displaystyle set at recurlevel */
        isdisplaystyle = value;
        break;
    case ISSUPER:              /* set supersampling/lowpass flag */
#ifndef SSFONTS                 /* don't have ss fonts loaded */
        value = 0;              /* so force lowpass */
#endif
        isss = issupersampling = value;
        fonttable = (issupersampling ? ssfonttable : aafonttable);      /* set fonts */
        break;
    case ISFONTSIZE:           /* set fontsize */
    case ISMAGSTEP:            /* set magstep */
    case ISDISPLAYSIZE:        /* set displaysize */
    case ISSHRINK:             /* set shrinkfactor */
    case ISAAALGORITHM:        /* set anti-aliasing algorithm */
    case ISWEIGHT:             /* set font weight */
    case ISCENTERWT:           /* set lowpass center pixel weight */
    case ISADJACENTWT:         /* set lowpass adjacent weight */
    case ISCORNERWT:           /* set lowpass corner weight */
    case ISSMASH:              /* set (minimum) "smash" margin */
//  case ISGAMMA:               /* set gamma correction */
//  case ISPBMPGM:              /* set pbmpgm output flag and ptype */
        if (value != NOVALUE) { /* passed a fixed value to be set */
            argvalue = value;   /* set given fixed int value */
            /* not used --dblvalue = (double) value; */
        } /* or maybe interpreted as double */
        else {                  /* get value from expression */
            *expression =
                texsubexpr(*expression, valuearg, 1023, "{", "}", 0, 0);
            if (*valuearg != '\0') {    /* guard against empty string */
                if (!isalpha(*valuearg)) {      /* and against alpha string args */
                    if (!isthischar(*valuearg, "?")) {  /*leading ? is query for value */
                        isdelta = isthischar(*valuearg, "+-");  /* leading + or - */
                        if (memcmp(valuearg, "--", 2) == 0) {   /* leading -- signals... */
                            isdelta = 0;
                            strsqueeze(valuearg, 1);
                        }       /* ...not delta */
                        switch (flag) { /* convert to double or int */
                        default:
                            argvalue = atoi(valuearg);
                            break;      /* convert to int */
                        }       /* or to double */
                    }
                }
            }
        }
        switch (flag) {
        default:
            break;
        case ISFONTSIZE:       /* set fontsize */
            if (argvalue != NOVALUE) {  /* got a value */
                int largestsize = (issupersampling ? 16 : LARGESTSIZE);
                fontsize = (isdelta ? fontsize + argvalue : argvalue);
                fontsize = max2(0, min2(fontsize, largestsize));
                shrinkfactor = shrinkfactors[fontsize];
                if (isdisplaystyle == 1 /* displaystyle enabled but not set */
                    || (1 && isdisplaystyle == 2)       /* displaystyle enabled and set */
                    ||(0 && isdisplaystyle == 0))       /*\textstyle disabled displaystyle */
                    if (displaystylelevel != recurlevel)        /*respect \displaystyle */
                        if (!ispreambledollars) {       /* respect $$...$$'s */
                            if (fontsize >= displaysize)
                                isdisplaystyle = 2;     /* forced */
                            else
                                isdisplaystyle = 1;
                        }
                /*displaystylelevel = (-99); */
            } /* reset \displaystyle level */
            else {              /* embed font size in expression */
                sprintf(valuearg, "%d", fontsize);      /* convert size */
                valuelen = strlen(valuearg);    /* ought to be 1 */
                if (*expression != '\0') {      /* ill-formed expression */
                    *expression = (*expression - valuelen);     /*back up buff */
                    // XXX:
                    memcpy((char *) (*expression), valuearg, valuelen);
                }
            }
            break;
        case ISMAGSTEP:        /* set magstep */
            if (argvalue != NOVALUE) {  /* got a value */
                int largestmag = 10;
                magstep = (isdelta ? magstep + argvalue : argvalue);
                magstep = max2(1, min2(magstep, largestmag));
            }
            break;
        case ISDISPLAYSIZE:    /* set displaysize */
            if (argvalue != NOVALUE)    /* got a value */
                displaysize =
                    (isdelta ? displaysize + argvalue : argvalue);
            break;
        case ISSMASH:          /* set (minimum) "smash" margin */
            if (argvalue != NOVALUE) {  /* got a value */
                smashmargin = argvalue; /* set value */
                if (arg3 != NOVALUE)
                    isdelta = arg3;     /* hard-coded isdelta */
                issmashdelta = (isdelta ? 1 : 0);
            }                   /* and set delta flag */
            smashmargin = max2((isdelta ? -5 : 0), min2(smashmargin, 32));      /*sanity */
            isexplicitsmash = 1;        /* signal explicit \smash directive */
            break;
        case ISSHRINK:         /* set shrinkfactor */
            if (argvalue != NOVALUE)    /* got a value */
                shrinkfactor =
                    (isdelta ? shrinkfactor + argvalue : argvalue);
            shrinkfactor = max2(1, min2(shrinkfactor, 27));     /* sanity check */
            break;
        case ISAAALGORITHM:    /* set anti-aliasing algorithm */
            if (argvalue != NOVALUE) {  /* got a value */
                if (argvalue >= 0) {    /* non-negative to set algorithm */
                    aaalgorithm = argvalue;     /* set algorithm number */
                    aaalgorithm = max2(0, min2(aaalgorithm, 4));
                } else {
                    /* bounds check */
                    maxfollow = abs(argvalue);
                }
            }                   /* or maxfollow=abs(negative#) */
            break;
        case ISWEIGHT:         /* set font weight number */
            value = (argvalue == NOVALUE ? NOVALUE :    /* don't have a value */
                     (isdelta ? weightnum + argvalue : argvalue));
            if (value >= 0 && value < maxaaparams) {    /* in range */
                weightnum = value;      /* reset weightnum index */
                minadjacent = aaparams[weightnum].minadjacent;
                maxadjacent = aaparams[weightnum].maxadjacent;
                cornerwt = aaparams[weightnum].cornerwt;
                adjacentwt = aaparams[weightnum].adjacentwt;
                centerwt = aaparams[weightnum].centerwt;
                fgalias = aaparams[weightnum].fgalias;
                fgonly = aaparams[weightnum].fgonly;
                bgalias = aaparams[weightnum].bgalias;
                bgonly = aaparams[weightnum].bgonly;
            }
            break;
        case ISCENTERWT:       /* set lowpass center pixel weight */
            if (argvalue != NOVALUE) {  /* got a value */
                centerwt = argvalue;    /* set lowpass center weight */
            }
            break;
        case ISADJACENTWT:     /* set lowpass adjacent weight */
            if (argvalue != NOVALUE) {  /* got a value */
                adjacentwt = argvalue;  /* set lowpass adjacent weight */
            }
            break;
        case ISCORNERWT:       /* set lowpass corner weight */
            if (argvalue != NOVALUE) {  /* got a value */
                cornerwt = argvalue;    /* set lowpass corner weight */
            }
            break;
        }
        break;
    case UNITLENGTH:
        if (value != NOVALUE)   /* passed a fixed value to be set */
            unitlength = (double) (value);      /* set given fixed value */
        else {                  /* get value from expression */
            *expression =
                texsubexpr(*expression, valuearg, 1023, "{", "}", 0, 0);
            if (*valuearg != '\0')      /* guard against empty string */
                unitlength = strtod(valuearg, NULL);
        }                       /* convert to double */
        iunitlength = (int) (unitlength + 0.5); /* iunitlength reset */
        break;
    }
    return NULL;
}

/* ==========================================================================
 * Function:    rastspace(expression, size, basesp,  width, isfill, isheight)
 * Purpose: returns a blank/space subraster width wide,
 *      with baseline and height corresponding to basep
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char **  to first char of null-terminated
 *              LaTeX expression (unused/unchanged)
 *      size (I)    int containing base font size (not used,
 *              just stored in subraster)
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding space, whose baseline
 *              and height params are transferred to space
 *      width (I)   int containing #bits/pixels for space width
 *      isfill (I)  int containing true to \hfill complete
 *              expression out to width
 *              (Kludge: isfill=99 signals \hspace*
 *              for negative space)
 *      isheight (I)    int containing true (but not NOVALUE)
 *              to treat width arg as height
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to empty/blank subraster
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *rastspace(const char **expression, int size,
                            subraster * basesp, int width, int isfill,
                            int isheight) {
    subraster *spacesp = NULL;  /* subraster for space */
    raster *bp = NULL;          /* for negative space */
    int baseht = 1, baseln = 0; /* height,baseline of base symbol */
    int pixsz = 1;              /*default #bits per pixel, 1=bitmap */
    int isstar = 0, minspace = 0;       /* defaults for negative hspace */
    char widtharg[256];         /* parse for optional {width} */
    int evalue = 0;             /* evaluate [args], {args} */
    subraster *rightsp = NULL;  /*rasterize right half of expression */
/* -------------------------------------------------------------------------
initialization
-------------------------------------------------------------------------- */
    if (isfill > 1) {
        isstar = 1;
        isfill = 0;
    } /* large fill signals \hspace* */ if (isfill == NOVALUE) {
        isfill = 0;             /* novalue means false */
    }
    if (isheight == NOVALUE) {
        isheight = 0;           /* novalue means false */
    }
    minspace = (isstar ? (-1) : 0);     /* reset default minspace */
/* -------------------------------------------------------------------------
determine width if not given (e.g., \hspace{width}, \hfill{width})
-------------------------------------------------------------------------- */
    if (width == 0) {           /* width specified in expression */
        double dwidth;
        int widthval;           /* test {width} before using it */
        int minwidth = (isfill || isheight ? 1 : -600); /* \hspace allows negative */
        /* --- check if optional [minspace] given for negative \hspace --- */
        if (*(*expression) == '[') {    /* [minspace] if leading char is [ */
            /* ---parse [minspace], bump expression past it, evaluate expression--- */
            *expression =
                texsubexpr(*expression, widtharg, 127, "[", "]", 0, 0);
            if (!isempty(widtharg)) {   /* got [minspace] */
                evalue = evalterm(mimestore, widtharg); /* evaluate widtharg expr */
                minspace = iround(unitlength * ((double) evalue));
            }                   /* in pixels */
        }                       /* --- end-of-if(*(*expression)=='[') --- */
        width = 1;              /* set default width */
        *expression =
            texsubexpr(*expression, widtharg, 255, "{", "}", 0, 0);
        dwidth = unitlength * ((double) evalterm(mimestore, widtharg)); /* scaled */
        widthval =              /* convert {width} to integer */
            (int) (dwidth + (dwidth >= 0.0 ? 0.5 : (-0.5)));
        if (widthval >= minwidth && widthval <= 600)    /* sanity check */
            width = widthval;   /* replace deafault width */
    }
    /* -------------------------------------------------------------------------
       first check for negative space
       -------------------------------------------------------------------------- */
    if (width < 0) {            /* have negative hspace */
        if (leftexpression != (subraster *) NULL)       /* can't backspace */
            if ((spacesp = new_subraster(0, 0, 0))      /* get new subraster for backspace */
                !=NULL) {       /* and if we succeed... */
                int nback = (-width), pback;    /*#pixels wanted,actually backspaced */
                if ((bp =
                     backspace_raster(leftexpression->image, nback, &pback,
                                      minspace, 0))
                    != NULL) {  /* and if backspace succeeds... */
                    spacesp->image = bp;        /* save backspaced image */
                    /*spacesp->type = leftexpression->type; *//* copy original type */
                    spacesp->type = blanksignal;        /* need to propagate blanks */
                    spacesp->size = leftexpression->size;       /* copy original font size */
                    spacesp->baseline = leftexpression->baseline;       /* and baseline */
                    blanksymspace += -(nback - pback);  /* wanted more than we got */
                    isreplaceleft = 1;
                } /*signal to replace entire expressn */
                else {          /* backspace failed */
                    delete_subraster(spacesp);  /* free unneeded envelope */
                    spacesp = (subraster *) NULL;
                }
            }                   /* and signal failure */
        goto end_of_job;
    }
    /* --- end-of-if(width<0) --- */
    /* -------------------------------------------------------------------------
       see if width is "absolute" or fill width
       -------------------------------------------------------------------------- */
    if (isfill                  /* called as \hfill{} */
        && !isheight) {         /* parameter conflict */
        if (leftexpression != NULL)     /* if we have left half */
            width -= (leftexpression->image)->width;    /*reduce left width from total */
        if ((rightsp = rasterize(*expression, size))    /* rasterize right half */
            !=NULL)             /* succeeded */
            width -= (rightsp->image)->width;
    }

    /* reduce right width from total */
    /* -------------------------------------------------------------------------
       construct blank subraster, and return it to caller
       -------------------------------------------------------------------------- */
    /* --- get parameters from base symbol --- */
    if (basesp != (subraster *) NULL) { /* we have base symbol for space */
        baseht = (basesp->image)->height;       /* height of base symbol */
        baseln = basesp->baseline;
    }
    /* and its baseline */
    /* --- flip params for height --- */
    if (isheight) {             /* width is actually height */
        baseht = width;         /* use given width as height */
        width = 1;
    }
    /* and set default width */
    /* --- generate and init space subraster --- */
    if (width > 0)              /*make sure we have positive width */
        if ((spacesp = new_subraster(width, baseht, pixsz))     /*generate space subraster */
            !=NULL) {           /* and if we succeed... *//* --- ...re-init subraster parameters --- */
            spacesp->size = size;       /*propagate base font size forward */
            if (1)
                spacesp->type = blanksignal;    /* need to propagate blanks (???) */
            spacesp->baseline = baseln;
        }
    /* ditto baseline */
    /* -------------------------------------------------------------------------
       concat right half if \hfill-ing
       -------------------------------------------------------------------------- */
    if (rightsp != NULL) {      /* we have a right half after fill */
        spacesp = (spacesp == NULL ? rightsp :  /* no space, so just use right half */
                   rastcat(spacesp, rightsp, 3));       /* or cat right half after space */
        spacesp->type = blanksignal;    /* need to propagate blanks */
        *expression += strlen((*expression));
    }                           /* push expression to its null */
  end_of_job:
    return spacesp;
}

/* ==========================================================================
 * Function:    rastnewline ( expression, size, basesp,  arg1, arg2, arg3 )
 * Purpose: \\ handler, returns subraster corresponding to
 *      left-hand expression preceding \\ above right-hand expression
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \\ to be
 *              rasterized, and returning ptr immediately
 *              to terminating null.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \\
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to expression,
 *              or NULL for any parsing error
 *              (expression ptr unchanged if error occurs)
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *rastnewline(const char **expression, int size,
                              subraster * basesp, int arg1, int arg2,
                              int arg3) {
    subraster *newlsp = NULL;   /* subraster for both lines */
    subraster *rightsp = NULL;  /*rasterize right half of expression */
    char spacexpr[129] /*, *xptr=spacexpr */ ;  /*for \\[vspace] */
    int evalue = 0;             /* evaluate [arg], {arg} */
    int vspace = size + 2;      /* #pixels between lines */
/* -------------------------------------------------------------------------
obtain optional [vspace] argument immediately following \\ command
-------------------------------------------------------------------------- */
/* --- check if [vspace] given --- */
    if (*(*expression) == '[') {        /*have [vspace] if leading char is [ */
        /* ---parse [vspace] and bump expression past it, interpret as double--- */
        *expression =
            texsubexpr(*expression, spacexpr, 127, "[", "]", 0, 0);
        if (*spacexpr == '\0') {
            goto end_of_job;    /* couldn't get [vspace] */
        }
        evalue = evalterm(mimestore, spacexpr); /* evaluate [space] arg */
        vspace = iround(unitlength * ((double) evalue));        /* vspace in pixels */
    }                           /* --- end-of-if(*(*expression)=='[') --- */
    if (leftexpression == NULL) {
        goto end_of_job;        /* nothing preceding \\ */
    }
/* -------------------------------------------------------------------------
rasterize right half of expression and stack left half above it
-------------------------------------------------------------------------- */
/* --- rasterize right half --- */
    if ((rightsp = rasterize(*expression, size)) == NULL) {
        goto end_of_job;        /* quit if failed */
    }
/* --- stack left half above it --- */
    /*newlsp = rastack(rightsp,leftexpression,1,vspace,0,3); *//*right under left */
    newlsp = rastack(rightsp, leftexpression, 1, vspace, 0, 1); /*right under left */
/* --- back to caller --- */
  end_of_job:
    if (newlsp != NULL) {       /* returning entire expression */
        int newht = (newlsp->image)->height;    /* height of returned subraster */
        newlsp->baseline = min2(newht - 1, newht / 2 + 5);      /* guess new baseline */
        isreplaceleft = 1;      /* so set flag to replace left half */
        *expression += strlen(*expression);
    }                           /* and push to terminating null */
    return newlsp;              /* 1st line over 2nd, or null=error */
}

/* ==========================================================================
 * Function:    rastarrow ( expression, size, basesp,  drctn, isBig, arg3 )
 * Purpose: returns left/right arrow subraster (e.g., for \longrightarrow)
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char **  to first char of null-terminated
 *              LaTeX expression (unused/unchanged)
 *      size (I)    int containing base font size (not used,
 *              just stored in subraster)
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding space, whose baseline
 *              and height params are transferred to space
 *      drctn (I)   int containing +1 for right, -1 for left,
 *              or 0 for leftright
 *      isBig (I)   int containing 0 for ---> or 1 for ===>
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to left/right arrow subraster
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o An optional argument [width] may *immediately* follow
 *      the \longxxx to explicitly set the arrow's width in pixels.
 *      For example, \longrightarrow calculates a default width
 *      (as usual in LaTeX), whereas \longrightarrow[50] explicitly
 *      draws a 50-pixel long arrow.  This can be used, e.g.,
 *      to draw commutative diagrams in conjunction with
 *      \array (and maybe with \stackrel and/or \relstack, too).
 *        o In case you really want to render, say, [f]---->[g], just
 *      use an intervening space, i.e., [f]\longrightarrow~[g].
 *      In text mode use two spaces {\rm~[f]\longrightarrow~~[g]}.
 * ======================================================================= */
static subraster *rastarrow(const char **expression, int size,
                            subraster * basesp, int drctn, int isBig,
                            int arg3) {
    subraster *arrowsp = NULL;  /* subraster for arrow */
    char widtharg[256];         /* parse for optional [width] */
    char sub[1024], super[1024];        /* and _^limits after [width] */
    subraster *subsp = NULL, *supsp = NULL;     /*rasterize limits */
    subraster *spacesp = NULL;  /*space below arrow */
    int width = 10 + 8 * size, height;  /* width, height for \longxxxarrow */
    int islimits = 1;           /*true to handle limits internally */
    int limsize = size - 1;     /* font size for limits */
    int vspace = 1;             /* #empty rows below arrow */
    int pixsz = 1;              /*default #bits per pixel, 1=bitmap */
/* -------------------------------------------------------------------------
construct longleft/rightarrow subraster, with limits, and return it to caller
-------------------------------------------------------------------------- */
/* --- check for optional width arg and replace default width --- */
    if (*(*expression) == '[') {        /*check for []-enclosed optional arg */
        int widthval;           /* test [width] before using it */
        *expression =
            texsubexpr(*expression, widtharg, 255, "[", "]", 0, 0);
         widthval =
            (int) ((unitlength *
                    ((double) evalterm(mimestore, widtharg))) + 0.5);
        if (widthval >= 2 && widthval <= 600)   /* sanity check */
             width = widthval;
    }
                                                                                                                        /* replace deafault width *//* --- now parse for limits, and bump expression past it(them) --- */ if (islimits) {
                                                                                                                        /* handling limits internally */
        *expression = texscripts(*expression, sub, super, 3);   /* parse for limits */
        if (*sub != '\0')       /*have a subscript following arrow */
            subsp = rasterize(sub, limsize);    /* so try to rasterize subscript */
        if (*super != '\0')     /*have superscript following arrow */
            supsp = rasterize(super, limsize);
    }
    /*so try to rasterize superscript */
    /* --- set height based on width --- */
    height = min2(17, max2(9, (width + 2) / 6));        /* height based on width */
    height = 1 + (height / 2) * 2;      /* always force odd height */
/* --- generate arrow subraster --- */
    if ((arrowsp = arrow_subraster(width, height, pixsz, drctn, isBig)) /*build arrow */
        == NULL)
        goto end_of_job;        /* and quit if we failed */
/* --- add space below arrow --- */
    if (vspace > 0) {           /* if we have space below arrow */
        if ((spacesp = new_subraster(width, vspace, pixsz)) != NULL) {
            /* and if we succeeded */
            if ((arrowsp = rastack(spacesp, arrowsp, 2, 0, 1, 3)) == NULL) {
                goto end_of_job;        /* and quit if we failed */
            }
        }
    }
/* --- init arrow subraster parameters --- */
    arrowsp->size = size;       /*propagate base font size forward */
    arrowsp->baseline = height + vspace - 1;    /* set baseline at bottom of arrow */
/* --- add limits above/below arrow, as necessary --- */
    if (subsp != NULL) {        /* stack subscript below arrow */
        if ((arrowsp = rastack(subsp, arrowsp, 2, 0, 1, 3)) == NULL) {
            goto end_of_job;    /* quit if failed */
        }
    }
    if (supsp != NULL) {        /* stack superscript above arrow */
        if ((arrowsp = rastack(arrowsp, supsp, 1, vspace, 1, 3)) == NULL) {
            goto end_of_job;    /* quit if failed */
        }
    }
  end_of_job:
    return arrowsp;
}

/* ==========================================================================
 * Function:    rastuparrow ( expression, size, basesp,  drctn, isBig, arg3 )
 * Purpose: returns an up/down arrow subraster (e.g., for \longuparrow)
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char **  to first char of null-terminated
 *              LaTeX expression (unused/unchanged)
 *      size (I)    int containing base font size (not used,
 *              just stored in subraster)
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding space, whose baseline
 *              and height params are transferred to space
 *      drctn (I)   int containing +1 for up, -1 for down,
 *              or 0 for updown
 *      isBig (I)   int containing 0 for ---> or 1 for ===>
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to up/down arrow subraster
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o An optional argument [height] may *immediately* follow
 *      the \longxxx to explicitly set the arrow's height in pixels.
 *      For example, \longuparrow calculates a default height
 *      (as usual in LaTeX), whereas \longuparrow[25] explicitly
 *      draws a 25-pixel high arrow.  This can be used, e.g.,
 *      to draw commutative diagrams in conjunction with
 *      \array (and maybe with \stackrel and/or \relstack, too).
 *        o In case you really want to render, say, [f]---->[g], just
 *      use an intervening space, i.e., [f]\longuparrow~[g].
 *      In text use two spaces {\rm~[f]\longuparrow~~[g]}.
 * ======================================================================= */
static subraster *rastuparrow(const char **expression, int size,
                              subraster * basesp, int drctn, int isBig,
                              int arg3) {
    subraster *arrowsp = NULL;  /* subraster for arrow */
    char heightarg[256];        /* parse for optional [height] */
    char sub[1024], super[1024];        /* and _^limits after [width] */
    subraster *subsp = NULL, *supsp = NULL;     /*rasterize limits */
    int height = 8 + 2 * size, width;   /* height, width for \longxxxarrow */
    int islimits = 1;           /*true to handle limits internally */
    int limsize = size - 1;     /* font size for limits */
    int pixsz = 1;              /*default #bits per pixel, 1=bitmap */
/* -------------------------------------------------------------------------
construct blank subraster, and return it to caller
-------------------------------------------------------------------------- */
/* --- check for optional height arg and replace default height --- */
    if (*(*expression) == '[') {        /*check for []-enclosed optional arg */
        int heightval;          /* test height before using it */
        *expression =
            texsubexpr(*expression, heightarg, 255, "[", "]", 0, 0);
         heightval =
            (int) ((unitlength *
                    ((double) evalterm(mimestore, heightarg))) + 0.5);
        if (heightval >= 2 && heightval <= 600) {       /* sanity check */
            height = heightval;
        }
    }
                                                                                                                        /* replace deafault height *//* --- now parse for limits, and bump expression past it(them) --- */ if (islimits) {
                                                                                                                        /* handling limits internally */
        *expression = texscripts(*expression, sub, super, 3);   /* parse for limits */
        if (*sub != '\0')       /*have a subscript following arrow */
            subsp = rasterize(sub, limsize);    /* so try to rasterize subscript */
        if (*super != '\0')     /*have superscript following arrow */
            supsp = rasterize(super, limsize);
    }
    /*so try to rasterize superscript */
    /* --- set width based on height --- */
    width = min2(17, max2(9, (height + 2) / 4));        /* width based on height */
    width = 1 + (width / 2) * 2;        /* always force odd width */
/* --- generate arrow subraster --- */
    if ((arrowsp =
         uparrow_subraster(width, height, pixsz, drctn, isBig)) == NULL)
        goto end_of_job;        /* and quit if we failed */
/* --- init arrow subraster parameters --- */
    arrowsp->size = size;       /*propagate base font size forward */
    arrowsp->baseline = height - 1;     /* set baseline at bottom of arrow */
/* --- add limits above/below arrow, as necessary --- */
    if (supsp != NULL) {        /* cat superscript to left of arrow */
        int supht = (supsp->image)->height,     /* superscript height */
            deltab = (1 + abs(height - supht)) / 2;     /* baseline difference to center */
        supsp->baseline = supht - 1;    /* force script baseline to bottom */
        if (supht <= height)    /* arrow usually taller than script */
            arrowsp->baseline -= deltab;        /* so bottom of script goes here */
        else
            supsp->baseline -= deltab;  /* else bottom of arrow goes here */
        if ((arrowsp = rastcat(supsp, arrowsp, 3))      /* superscript left of arrow */
            == NULL)
            goto end_of_job;
    }                           /* quit if failed */
    if (subsp != NULL) {        /* cat subscript to right of arrow */
        int subht = (subsp->image)->height,     /* subscript height */
            deltab = (1 + abs(height - subht)) / 2;     /* baseline difference to center */
        arrowsp->baseline = height - 1; /* reset arrow baseline to bottom */
        subsp->baseline = subht - 1;    /* force script baseline to bottom */
        if (subht <= height)    /* arrow usually taller than script */
            arrowsp->baseline -= deltab;        /* so bottom of script goes here */
        else
            subsp->baseline -= deltab;  /* else bottom of arrow goes here */
        if ((arrowsp = rastcat(arrowsp, subsp, 3))      /* subscript right of arrow */
            == NULL)
            goto end_of_job;
    }
    /* quit if failed */
    /* --- return arrow (or NULL) to caller --- */
  end_of_job:
    arrowsp->baseline = height - 1;     /* reset arrow baseline to bottom */
    return arrowsp;
}

/* ==========================================================================
 * Function:    rastoverlay (expression, size, basesp, overlay, offset2, arg3)
 * Purpose: overlays one raster on another
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following overlay \cmd to
 *              be rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding overlay \cmd
 *              (unused, but passed for consistency)
 *      overlay (I) int containing 1 to overlay / (e.g., \not)
 *              or NOVALUE to pick up 2nd arg from expression
 *      offset2 (I) int containing #pixels to horizontally offset
 *              overlay relative to underlying symbol,
 *              positive(right) or negative or 0,
 *              or NOVALUE to pick up optional [offset] arg
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to composite,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *rastoverlay(const char **expression, int size,
                              subraster * basesp, int overlay, int offset2,
                              int arg3) {
    char expr1[512], expr2[512];        /* base, overlay */
    subraster *sp1 = NULL, *sp2 = NULL; /*rasterize 1=base, 2=overlay */
    subraster *overlaysp = NULL;        /*subraster for composite overlay */
    int isalign = 0;            /* true to align baselines */
/* -------------------------------------------------------------------------
Obtain base, and maybe overlay, and rasterize them
-------------------------------------------------------------------------- */
/* --- check for optional offset2 arg  --- */
    if (offset2 == NOVALUE)     /* only if not explicitly specified */
        if (*(*expression) == '[') {    /*check for []-enclosed optional arg */
            int offsetval;      /* test before using it */
            *expression =
                texsubexpr(*expression, expr2, 511, "[", "]", 0, 0);
             offsetval =        /* convert [offset2] to int */
                (int) (((double) evalterm(mimestore, expr2)) + 0.5);
            if (abs(offsetval) <= 25)   /* sanity check */
                 offset2 = offsetval;
    } /* replace deafault */ if (offset2 == NOVALUE)
        offset2 = 0;            /* novalue means no offset */
/* --- parse for base, bump expression past it, and rasterize it --- */
    *expression = texsubexpr(*expression, expr1, 511, "{", "}", 0, 0);
    if (isempty(expr1))
        goto end_of_job;        /* nothing to overlay, so quit */
    rastlift1 = rastlift = 0;   /* reset all raisebox() lifts */
    if (strstr(expr1, "\\raise") != NULL)       /* have a \raisebox */
        isalign = 2;            /* so align baselines */
    if ((sp1 = rasterize(expr1, size))  /* rasterize base expression */
        == NULL)
        goto end_of_job;        /* quit if failed to rasterize */
    overlaysp = sp1;            /*in case we return with no overlay */
    rastlift1 = rastlift;       /* save lift for base expression */
/* --- get overlay expression, and rasterize it --- */
    if (overlay == NOVALUE) {   /* get overlay from input stream */
        *expression = texsubexpr(*expression, expr2, 511, "{", "}", 0, 0);
        if (!isempty(expr2)) {  /* have an overlay */
            if (strstr(expr2, "\\raise") != NULL)       /* have a \raisebox */
                isalign = 2;    /* so align baselines */
            sp2 = rasterize(expr2, size);
        }
    } /* rasterize overlay expression */
    else                        /* use specific built-in overlay */
        switch (overlay) {
        default:
            break;
        case 1:                /* e.g., \not overlays slash */
            sp2 = rasterize("/", size + 1);     /* rasterize overlay expression */
            isalign = 0;        /* automatically handled correctly */
            offset2 = max2(1, size - 3);        /* push / right a bit */
            offset2 = 0;
            break;
        case 2:                /* e.g., \Not draws diagonal */
            sp2 = NULL;         /* no overlay required */
            isalign = 0;        /* automatically handled correctly */
            if (overlaysp != NULL) {    /* check that we have raster */
                raster *rp = overlaysp->image;  /* raster to be \Not-ed */
                int width = rp->width, height = rp->height;     /* raster dimensions */
                if (0)          /* diagonal within bounding box */
                    line_raster(rp, 0, width - 1, height - 1, 0, 1);    /* just draw diagonal */
                else {          /* construct "wide" diagonal */
                    int margin = 3;     /* desired extra margin width */
                    sp2 = new_subraster(width + margin, height + margin, 1);    /*alloc it */
                    if (sp2 != NULL)    /* allocated successfully */
                        line_raster(sp2->image, 0, width + margin - 1,
                                    height + margin - 1, 0, 1);
                }
            }
            break;
        case 3:                /* e.g., \sout for strikeout */
            sp2 = NULL;         /* no overlay required */
            if (overlaysp != NULL) {    /* check that we have raster */
                raster *rp = overlaysp->image;  /* raster to be \sout-ed */
                int width = rp->width, height = rp->height;     /* raster dimensions */
                int baseline = (overlaysp->baseline) - rastlift;        /*skip descenders */
                int midrow =
                    max2(0,
                         min2(height - 1, offset2 + ((baseline + 1) / 2)));
                if (1)          /* strikeout within bounding box */
                    line_raster(rp, midrow, 0, midrow, width - 1, 1);
            }                   /*draw strikeout */
            break;
        }                       /* --- end-of-switch(overlay) --- */
    if (sp2 == NULL)
        goto end_of_job;        /*return sp1 if failed to rasterize */
/* -------------------------------------------------------------------------
construct composite overlay
-------------------------------------------------------------------------- */
    overlaysp = rastcompose(sp1, sp2, offset2, isalign, 3);
  end_of_job:
    return overlaysp;
}

/* ==========================================================================
 * Function:    rastfrac ( expression, size, basesp,  isfrac, arg2, arg3 )
 * Purpose: \frac,\atop handler, returns a subraster corresponding to
 *      expression (immediately following \frac,\atop) at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \frac to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \frac
 *              (unused, but passed for consistency)
 *      isfrac (I)  int containing true to draw horizontal line
 *              between numerator and denominator,
 *              or false not to draw it (for \atop).
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to fraction,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
subraster *rastfrac(const char **expression, int size, subraster * basesp,
                    int isfrac, int arg2, int arg3) {
    char numer[MAXSUBXSZ + 1], denom[MAXSUBXSZ + 1];    /* parsed numer, denom */
    subraster *numsp = NULL, *densp = NULL;     /*rasterize numer, denom */
    subraster *fracsp = NULL;   /* subraster for numer/denom */
    int width = 0;              /* width of constructed raster */
    int numheight = 0;          /* height of numerator */
                                                        /* not used --int baseht = 0, baseln = 0 */ ;
                                                        /* height,baseline of base symbol */
    /*int   istweak = 1; *//*true to tweak baseline alignment */
    int lineheight = 1;         /* thickness of fraction line */
    int vspace = (size > 2 ? 2 : 1);    /*vertical space between components */
/* -------------------------------------------------------------------------
Obtain numerator and denominator, and rasterize them
-------------------------------------------------------------------------- */
/* --- parse for numerator,denominator and bump expression past them --- */
    *expression = texsubexpr(*expression, numer, 0, "{", "}", 0, 0);
    *expression = texsubexpr(*expression, denom, 0, "{", "}", 0, 0);
    if (*numer == '\0' && *denom == '\0')       /* missing both components of frac */
        goto end_of_job;        /* nothing to do, so quit */
/* --- rasterize numerator, denominator --- */
    if (*numer != '\0') {       /* have a numerator */
        if ((numsp = rasterize(numer, size - 1)) == NULL) {
            goto end_of_job;    /* and quit if failed */
        }
    }
    if (*denom != '\0') {       /* have a denominator */
        if ((densp = rasterize(denom, size - 1)) == NULL) {     /* failed */
            if (numsp != NULL) {        /* already rasterized numerator */
                delete_subraster(numsp);        /* so free now-unneeded numerator */
            }
            goto end_of_job;
        }
    }
    /* --- if one componenet missing, use a blank space for it --- */
    if (numsp == NULL) {        /* no numerator given */
        numsp = rasterize("[?]", size - 1);     /* missing numerator */
    }
    if (densp == NULL) {        /* no denominator given */
        densp = rasterize("[?]", size - 1);     /* missing denominator */
    }
/* --- check that we got both components --- */
    if (numsp == NULL || densp == NULL) {       /* some problem */
        delete_subraster(numsp);        /*delete numerator (if it existed) */
        delete_subraster(densp);        /*delete denominator (if it existed) */
        goto end_of_job;
    }
    /* and quit */
    /* --- get height of numerator (to determine where line belongs) --- */
    numheight = (numsp->image)->height; /* get numerator's height */
/* -------------------------------------------------------------------------
construct raster with numerator stacked over denominator
-------------------------------------------------------------------------- */
/* --- construct raster with numer/denom --- */
    if ((fracsp =
         rastack(densp, numsp, 0, 2 * vspace + lineheight, 1,
                 3)) == NULL) {
        /* failed to construct numer/denom */
        delete_subraster(numsp);        /* so free now-unneeded numerator */
        delete_subraster(densp);        /* and now-unneeded denominator */
        goto end_of_job;
    }
    /* and then quit */
    /* --- determine width of constructed raster --- */
    width = (fracsp->image)->width;     /*just get width of embedded image */
/* --- initialize subraster parameters --- */
    fracsp->size = size;        /* propagate font size forward */
    fracsp->baseline = (numheight + vspace + lineheight) + (size + 2);  /*default baseline */
    fracsp->type = FRACRASTER;  /* signal \frac image */
    if (basesp != NULL) {       /* we have base symbol for frac */
        /* not used --baseht = (basesp->image)->height; *//* height of base symbol */
        /* not used --baseln = basesp->baseline; *//* and its baseline */
    }
    /* -------------------------------------------------------------------------
       draw horizontal line between numerator and denominator
       -------------------------------------------------------------------------- */
    fraccenterline = numheight + vspace;        /* signal that we have a \frac */
    if (isfrac) {               /*line for \frac, but not for \atop */
        rule_raster(fracsp->image, fraccenterline, 0, width, lineheight,
                    0);
    }
/* -------------------------------------------------------------------------
return final result to caller
-------------------------------------------------------------------------- */
  end_of_job:
    if (msgfp != NULL && msglevel >= 99) {
        fprintf(msgfp, "rastfrac> returning %s\n",
                (fracsp == NULL ? "null" : "..."));
        if (fracsp != NULL)     /* have a constructed raster */
            type_raster(fracsp->image, msgfp);
    }                           /* display constructed raster */
    return fracsp;
}

/* ==========================================================================
 * Function:    rastackrel ( expression, size, basesp,  base, arg2, arg3 )
 * Purpose: \stackrel handler, returns a subraster corresponding to
 *      stacked relation
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \stackrel to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \stackrel
 *              (unused, but passed for consistency)
 *      base (I)    int containing 1 if upper/first subexpression
 *              is base relation, or 2 if lower/second is
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to stacked
 *              relation, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *rastackrel(const char **expression, int size,
                             subraster * basesp, int base, int arg2,
                             int arg3) {
    char upper[MAXSUBXSZ + 1], lower[MAXSUBXSZ + 1];    /* parsed upper, lower */
    subraster *upsp = NULL, *lowsp = NULL;      /* rasterize upper, lower */
    subraster *relsp = NULL;    /* subraster for upper/lower */
    int upsize = (base == 1 ? size : size - 1); /* font size for upper component */
    int lowsize = (base == 2 ? size : size - 1);        /* font size for lower component */
    int vspace = 1;             /*vertical space between components */
/* -------------------------------------------------------------------------
Obtain numerator and denominator, and rasterize them
-------------------------------------------------------------------------- */
/* --- parse for numerator,denominator and bump expression past them --- */
    *expression = texsubexpr(*expression, upper, 0, "{", "}", 0, 0);
    *expression = texsubexpr(*expression, lower, 0, "{", "}", 0, 0);
    if (*upper == '\0' || *lower == '\0') {     /* missing either component */
        goto end_of_job;        /* nothing to do, so quit */
    }
                                                                /* --- rasterize upper, lower --- */ if (*upper != '\0') {
                                                                /* have upper component */
        if ((upsp = rasterize(upper, upsize)) == NULL) {
            goto end_of_job;    /* and quit if failed */
        }
    }
    if (*lower != '\0') {       /* have lower component */
        if ((lowsp = rasterize(lower, lowsize)) == NULL) {
            /* failed */
            if (upsp != NULL) { /* already rasterized upper */
                delete_subraster(upsp); /* so free now-unneeded upper */
            }
            goto end_of_job;
        }
    }
    /* -------------------------------------------------------------------------
       construct stacked relation raster
       -------------------------------------------------------------------------- */
    /* --- construct stacked relation --- */
    if ((relsp = rastack(lowsp, upsp, 3 - base, vspace, 1, 3)) == NULL) {
        goto end_of_job;        /* quit if failed */
    }
    relsp->size = size;         /* propagate font size forward */
  end_of_job:
    return (relsp);
}

/* ==========================================================================
 * Function:    rastmathfunc ( expression, size, basesp,  base, arg2, arg3 )
 * Purpose: \log, \lim, etc handler, returns a subraster corresponding
 *      to math functions
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \mathfunc to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \mathfunc
 *              (unused, but passed for consistency)
 *      mathfunc (I)    int containing 1=arccos, 2=arcsin, etc.
 *      islimits (I)    int containing 1 if function may have
 *              limits underneath, e.g., \lim_{n\to\infty}
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to mathfunc,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *rastmathfunc(const char **expression, int size,
                               subraster * basesp, int mathfunc,
                               int islimits, int arg3) {
    char func[MAXTOKNSZ + 1], limits[MAXSUBXSZ + 1];    /*func as {\rm func}, limits */
    char funcarg[MAXTOKNSZ + 1];        /* optional func arg */
    subraster *funcsp = NULL, *limsp = NULL;    /*rasterize func,limits */
    subraster *mathfuncsp = NULL;       /* subraster for mathfunc/limits */
    int limsize = size - 1;     /* font size for limits */
    int vspace = 1;             /*vertical space between components */
/* --- table of function names by mathfunc number --- */
    static const char *funcnames[] = {
        "error",                /*  0 index is illegal/error bucket */
            "arccos", "arcsin", "arctan",       /*  1 -  3 */
            "arg", "cos", "cosh",       /*  4 -  6 */
            "cot", "coth", "csc",       /*  7 -  9 */
            "deg", "det", "dim",        /* 10 - 12 */
            "exp", "gcd", "hom",        /* 13 - 15 */
            "inf", "ker", "lg", /* 16 - 18 */
            "lim", "liminf", "limsup",  /* 19 - 21 */
            "ln", "log", "max", /* 22 - 24 */
            "min", "Pr", "sec", /* 25 - 27 */
            "sin", "sinh", "sup",       /* 28 - 30 */
            "tan", "tanh",      /* 31 - 32 */
            /* --- extra mimetex funcnames --- */
            "tr",               /* 33 */
            "pmod"              /* 34 */
    };
    static int numnames = sizeof(funcnames) / sizeof(funcnames[0]);
/* -------------------------------------------------------------------------
set up and rasterize function name in \rm
-------------------------------------------------------------------------- */
    if (mathfunc < 0 || mathfunc > numnames) {
        mathfunc = 0;           /* check index bounds */
    }
    switch (mathfunc) {         /* check for special processing */
    default:                   /* no special processing */
        strcpy(func, "{\\rm~"); /* init string with {\rm~ */
        strcat(func, funcnames[mathfunc]);      /* concat function name */
        strcat(func, "}");      /* and add terminating } */
        break;
    case 34:                   /* \pmod{x} --> (mod x) */
        /* --- parse for \pmod{arg} argument --- */
        *expression =
            texsubexpr(*expression, funcarg, 2047, "{", "}", 0, 0);
        strcpy(func, "{\\({\\rm~mod}"); /* init with {\left({\rm~mod} */
        strcat(func, "\\hspace2");      /* concat space */
        strcat(func, funcarg);  /* and \pmodargument */
        strcat(func, "\\)}");   /* and add terminating \right)} */
        break;
    }
    if ((funcsp = rasterize(func, size)) == NULL) {
        goto end_of_job;
    }
    mathfuncsp = funcsp;        /* just return funcsp if no limits */
    if (!islimits) {
        goto end_of_job;        /* treat any subscript normally */
    }
/* -------------------------------------------------------------------------
Obtain limits, if permitted and if provided, and rasterize them
-------------------------------------------------------------------------- */
/* --- parse for subscript limits, and bump expression past it(them) --- */
    *expression = texscripts(*expression, limits, limits, 1);
    if (*limits == '\0') {
        goto end_of_job;        /* no limits, nothing to do, quit */
    }
/* --- rasterize limits --- */
    if ((limsp = rasterize(limits, limsize)) == NULL) {
        goto end_of_job;        /* and quit if failed */
    }
/* -------------------------------------------------------------------------
construct func atop limits
-------------------------------------------------------------------------- */
/* --- construct func atop limits --- */
    if ((mathfuncsp = rastack(limsp, funcsp, 2, vspace, 1, 3)) == NULL) {
        goto end_of_job;        /* quit if failed */
    }
/* --- initialize subraster parameters --- */
    mathfuncsp->size = size;    /* propagate font size forward */
/* -------------------------------------------------------------------------
return final result to caller
-------------------------------------------------------------------------- */
  end_of_job:
    return mathfuncsp;
}

/* ==========================================================================
 * Function:    rastsqrt ( expression, size, basesp,  arg1, arg2, arg3 )
 * Purpose: \sqrt handler, returns a subraster corresponding to
 *      expression (immediately following \sqrt) at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \sqrt to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \accent
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to expression,
 *              or NULL for any parsing error
 *              (expression ptr unchanged if error occurs)
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *rastsqrt(const char **expression, int size,
                           subraster * basesp, int arg1, int arg2,
                           int arg3) {
    char subexpr[MAXSUBXSZ + 1];        /*parse subexpr to be sqrt-ed */
    char rootarg[MAXSUBXSZ + 1];        /* optional \sqrt[rootarg]{...} */
    subraster *subsp = NULL;    /* rasterize subexpr */
    subraster *sqrtsp = NULL;   /* subraster with the sqrt */
    subraster *rootsp = NULL;   /* optionally preceded by [rootarg] */
    int sqrtheight = 0, sqrtwidth = 0, surdwidth = 0,   /* height,width of sqrt */
        rootheight = 0, rootwidth = 0,  /* height,width of rootarg raster */
        subheight = 0, subwidth = 0, pixsz = 0; /* height,width,pixsz of subexpr */
    int overspace = 2;          /*space between subexpr and overbar */
/* -------------------------------------------------------------------------
Obtain subexpression to be sqrt-ed, and rasterize it
-------------------------------------------------------------------------- */
/* --- first check for optional \sqrt[rootarg]{...} --- */
    if (*(*expression) == '[') {        /*check for []-enclosed optional arg */
        *expression = texsubexpr(*expression, rootarg, 0, "[", "]", 0, 0);
        if (*rootarg != '\0')   /* got rootarg */
            if ((rootsp = rasterize(rootarg, size - 1)) /*rasterize it at smaller size */
                !=NULL) {       /* rasterized successfully */
                rootheight = (rootsp->image)->height;   /* get height of rootarg */
                rootwidth = (rootsp->image)->width;
    } /* and its width */ }
    /* --- end-of-if(**expression=='[') --- *//* --- parse for subexpr to be sqrt-ed, and bump expression past it --- */
        *expression = texsubexpr(*expression, subexpr, 0, "{", "}", 0, 0);
    if (*subexpr == '\0') {     /* couldn't get subexpression */
        goto end_of_job;        /* nothing to do, so quit */
    }
/* --- rasterize subexpression to be accented --- */
    if ((subsp = rasterize(subexpr, size)) == NULL) {
        goto end_of_job;        /* quit if failed */
    }
/* -------------------------------------------------------------------------
determine height and width of sqrt raster to be constructed
-------------------------------------------------------------------------- */
/* --- first get height and width of subexpr --- */
    subheight = (subsp->image)->height; /* height of subexpr */
    subwidth = (subsp->image)->width;   /* and its width */
    pixsz = (subsp->image)->pixsz;      /* pixsz remains constant */
/* --- determine height and width of sqrt to contain subexpr --- */
    sqrtheight = subheight + overspace; /* subexpr + blank line + overbar */
    surdwidth = SQRTWIDTH(sqrtheight, (rootheight < 1 ? 2 : 1));        /* width of surd */
    sqrtwidth = subwidth + surdwidth + 1;       /* total width */
/* -------------------------------------------------------------------------
construct sqrt (with room to move in subexpr) and embed subexpr in it
-------------------------------------------------------------------------- */
/* --- construct sqrt --- */
    if ((sqrtsp = accent_subraster(SQRTACCENT,
                                   (rootheight <
                                    1 ? sqrtwidth : (-sqrtwidth)),
                                   sqrtheight, 0, pixsz)) == NULL) {
        goto end_of_job;        /* quit if failed to build sqrt */
    }
/* --- embed subexpr in sqrt at lower-right corner--- */
    rastput(sqrtsp->image, subsp->image, overspace, sqrtwidth - subwidth,
            1);
    sqrtsp->baseline = subsp->baseline + overspace;     /* adjust baseline */
/* --- "embed" rootarg at upper-left --- */
    if (rootsp != NULL) {       /*have optional \sqrt[rootarg]{...} */
        /* --- allocate full raster to contain sqrtsp and rootsp --- */
        int fullwidth =
            sqrtwidth + rootwidth - min2(rootwidth,
                                         max2(0, surdwidth - 4));
        int fullheight =
            sqrtheight + rootheight - min2(rootheight, 3 + size);
        subraster *fullsp = new_subraster(fullwidth, fullheight, pixsz);
        if (fullsp != NULL) {   /* allocated successfully *//* --- embed sqrtsp exactly at lower-right corner --- */
            rastput(fullsp->image, sqrtsp->image,       /* exactly at lower-right corner */
                    fullheight - sqrtheight, fullwidth - sqrtwidth, 1);
            /* --- embed rootsp near upper-left, nestled above leading surd --- */
            rastput(fullsp->image, rootsp->image, 0,
                    max2(0, surdwidth - rootwidth - 2 - size), 0);
            /* --- replace sqrtsp with fullsp --- */
            delete_subraster(sqrtsp);   /* free original sqrtsp */
            sqrtsp = fullsp;    /* and repoint it to fullsp instead */
            sqrtsp->baseline = fullheight - (subheight - subsp->baseline);
        }
    }
    sqrtsp->size = size;        /* propagate font size forward */
/* -------------------------------------------------------------------------
free unneeded component subrasters and return final result to caller
-------------------------------------------------------------------------- */
  end_of_job:
    if (subsp != NULL) {
        delete_subraster(subsp);        /* free unneeded subexpr */
    }
    return sqrtsp;
}

/* ==========================================================================
 * Function:    rastaccent (expression,size,basesp,accent,isabove,isscript)
 * Purpose: \hat, \vec, \etc handler, returns a subraster corresponding
 *      to expression (immediately following \accent) at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \accent to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \accent
 *              (unused, but passed for consistency)
 *      accent (I)  int containing HATACCENT or VECACCENT, etc,
 *              between numerator and denominator,
 *              or false not to draw it (for \over).
 *      isabove (I) int containing true if accent is above
 *              expression to be accented, or false
 *              if accent is below (e.g., underbrace)
 *      isscript (I)    int containing true if sub/superscripts
 *              allowed (for under/overbrace), or 0 if not.
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to expression,
 *              or NULL for any parsing error
 *              (expression ptr unchanged if error occurs)
 * --------------------------------------------------------------------------
 * Notes:     o Also handles \overbrace{}^{} and \underbrace{}_{} by way
 *      of isabove and isscript args.
 * ======================================================================= */
static subraster *rastaccent(const char **expression, int size,
                             subraster * basesp, int accent, int isabove,
                             int isscript) {
    char subexpr[MAXSUBXSZ + 1];        /*parse subexpr to be accented */
    char *script = NULL;        /* \under,overbrace allow scripts */
    char subscript[MAXTOKNSZ + 1], supscript[MAXTOKNSZ + 1];    /* parsed scripts */
    subraster *subsp = NULL, *scrsp = NULL;     /*rasterize subexpr,script */
    subraster *accsubsp = NULL; /* stack accent, subexpr, script */
    subraster *accsp = NULL;    /*raster for the accent itself */
    int accheight = 0, accwidth = 0, accdir = 0,        /*accent height, width, direction */
        subheight = 0, subwidth = 0, pixsz = 0; /* height,width,pixsz of subexpr */
    int vspace = 0;             /*vertical space between accent,sub */
/* -------------------------------------------------------------------------
Obtain subexpression to be accented, and rasterize it
-------------------------------------------------------------------------- */
/* --- parse for subexpr to be accented, and bump expression past it --- */
    *expression = texsubexpr(*expression, subexpr, 0, "{", "}", 0, 0);
    if (*subexpr == '\0')       /* couldn't get subexpression */
        goto end_of_job;        /* nothing to do, so quit */
/* --- rasterize subexpression to be accented --- */
    if ((subsp = rasterize(subexpr, size))      /*rasterize subexpr at original size */
        == NULL)
        goto end_of_job;        /* quit if failed */
/* -------------------------------------------------------------------------
determine desired accent width and height
-------------------------------------------------------------------------- */
/* --- first get height and width of subexpr --- */
     subheight = (subsp->image)->height;        /* height of subexpr */
     subwidth = (subsp->image)->width;  /* and its width is overall width */
     pixsz = (subsp->image)->pixsz;     /* original pixsz remains constant */
/* --- determine desired width, height of accent --- */
     accwidth = subwidth;       /* same width as subexpr */
     accheight = 4;             /* default for bars */
    switch (accent) {
    default:
        break;                  /* default okay */
        case DOTACCENT:case DDOTACCENT:accheight = (size < 4 ? 3 : 4);  /* default for dots */
        break;
        case VECACCENT:vspace = 1;      /* set 1-pixel vertical space */
        accdir = isscript;      /* +1=right,-1=left,0=lr; +10for==> */
        isscript = 0;           /* >>don't<< signal sub/supscript */
        case HATACCENT:accheight = 7;   /* default */
        if (subwidth < 10)
            accheight = 5;      /* unless small width */
        else if (subwidth > 25)
            accheight = 9;      /* or large */
        break;
    } /* --- end-of-switch(accent) --- */ accheight = min2(accheight, subheight);       /*never higher than accented subexpr */
/* -------------------------------------------------------------------------
construct accent, and construct subraster with accent over (or under) subexpr
-------------------------------------------------------------------------- */
/* --- first construct accent --- */
    if ((accsp =
         accent_subraster(accent, accwidth, accheight, accdir, pixsz))
        == NULL)
        goto end_of_job;        /* quit if failed to build accent */
/* --- now stack accent above (or below) subexpr, and free both args --- */
    accsubsp = (isabove ? rastack(subsp, accsp, 1, vspace, 1, 3)        /*accent above subexpr */
                : rastack(accsp, subsp, 2, vspace, 1, 3));      /*accent below subexpr */
    if (accsubsp == NULL) {     /* failed to stack accent */
        delete_subraster(subsp);        /* free unneeded subsp */
        delete_subraster(accsp);        /* and unneeded accsp */
        goto end_of_job;
    }

    /* and quit */
    /* -------------------------------------------------------------------------
       look for super/subscript (annotation for over/underbrace)
       -------------------------------------------------------------------------- */
    /* --- first check whether accent permits accompanying annotations --- */
    if (!isscript)
        goto end_of_job;        /* no annotations for this accent */
/* --- now get scripts if there actually are any --- */
    *expression =
        texscripts(*expression, subscript, supscript, (isabove ? 2 : 1));
    script = (isabove ? supscript : subscript); /*select above^ or below_ script */
    if (*script == '\0')
        goto end_of_job;        /* no accompanying script */
/* --- rasterize script annotation at size-2 --- */
    if ((scrsp = rasterize(script, size - 2))   /* rasterize script at size-2 */
        == NULL)
        goto end_of_job;        /* quit if failed */
/* --- stack annotation above (or below) accent, and free both args --- */
    accsubsp = (isabove ? rastack(accsubsp, scrsp, 1, 0, 1, 3)  /* accent above base */
                : rastack(scrsp, accsubsp, 2, 0, 1, 3));        /* accent below base */
/* -------------------------------------------------------------------------
return final result to caller
-------------------------------------------------------------------------- */
  end_of_job:
    if (accsubsp != NULL)       /* initialize subraster parameters */
        accsubsp->size = size;  /* propagate font size forward */
    return (accsubsp);
}                               /* --- end-of-function rastaccent() --- */


/* ==========================================================================
 * Function:    rastfont (expression,size,basesp,ifontnum,arg2,arg3)
 * Purpose: \cal{}, \scr{}, \etc handler, returns subraster corresponding
 *      to char(s) within {}'s rendered at size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \font to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \accent
 *              (unused, but passed for consistency)
 *      ifontnum (I)    int containing 1 for \cal{}, 2 for \scr{}
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to chars
 *              between {}'s, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
subraster *rastfont(const char **expression, int size, subraster * basesp,
                    int ifontnum, int arg2, int arg3) {
    char fontchars[MAXSUBXSZ + 1];      /* chars to render in font */
    char subexpr[MAXSUBXSZ + 1];        /* turn \cal{AB} into \calA\calB */
    char *pfchars = fontchars;
    char fchar = '\0';          /* run thru fontchars one at a time */
    const char *name = NULL;    /* fontinfo[ifontnum].name */
    /* not used--int family = 0: *//* fontinfo[ifontnum].family */
    int istext = 0;             /* fontinfo[ifontnum].istext */
    int class = 0;              /* fontinfo[ifontnum].class */
    subraster *fontsp = NULL;   /* rasterize chars in font */
    int oldsmashmargin = smashmargin;   /* turn off smash in text mode */
#if 0
/* --- fonts recognized by rastfont --- */
    static int nfonts = 11;     /* legal font #'s are 1...nfonts */
    static struct {
        const char *name;
        int class;
    } fonts[] = {               /* --- name  class 1=upper,2=alpha,3=alnum,4=lower,5=digit,9=all --- */
        {
        "\\math", 0}, {
        "\\mathcal", 1},        /*(1) calligraphic, uppercase */
        {
        "\\mathscr", 1},        /*(2) rsfs/script, uppercase */
        {
        "\\textrm", -1},        /*(3) \rm,\text{abc} --> {\rm~abc} */
        {
        "\\textit", -1},        /*(4) \it,\textit{abc}-->{\it~abc} */
        {
        "\\mathbb", -1},        /*(5) \bb,\mathbb{abc}-->{\bb~abc} */
        {
        "\\mathbf", -1},        /*(6) \bf,\mathbf{abc}-->{\bf~abc} */
        {
        "\\mathrm", -1},        /*(7) \mathrm */
        {
        "\\cyr", -1},           /*(8) \cyr */
        {
        "\\textgreek", -1},     /*(9) \textgreek */
        {
        "\\textbfgreek", CMMI10BGR, 1, -1},     /*(10) \textbfgreek{ab} */
        {
        "\\textbbgreek", BBOLD10GR, 1, -1},     /*(11) \textbbgreek{ab} */
        {
        NULL, 0}
    };
#endif
/* -------------------------------------------------------------------------
first get font name and class to determine type of conversion desired
-------------------------------------------------------------------------- */
    if (ifontnum <= 0 || ifontnum > nfontinfo)
        ifontnum = 0;           /*math if out-of-bounds */
    name = fontinfo[ifontnum].name;     /* font name */
    /* not used --family = fontinfo[ifontnum].family; *//* font family */
    istext = fontinfo[ifontnum].istext; /*true in text mode (respect space) */
    class = fontinfo[ifontnum].class;   /* font class */
    if (istext) {               /* text (respect blanks) */
        mathsmashmargin = smashmargin;  /* needed for \text{if $n-m$ even} */
        smashmargin = 0;
    }
    /* don't smash internal blanks */
    /* -------------------------------------------------------------------------
       now convert \font{abc} --> {\font~abc}, or convert ABC to \calA\calB\calC
       -------------------------------------------------------------------------- */
    if (1 || class < 0) {       /* not character-by-character */
        /* ---
           if \font not immediately followed by { then it has no arg, so just set flag
           ------------------------------------------------------------------------ */
        if (*(*expression) != '{') {    /* no \font arg, so just set flag */
            if (msgfp != NULL && msglevel >= 99)
                fprintf(msgfp, "rastfont> \\%s rastflags() for font#%d\n",
                        name, ifontnum);
            fontsp =
                rastflags(expression, size, basesp, ISFONTFAM, ifontnum,
                          arg3);
            goto end_of_job;
        }

        /* --- end-of-if(*(*expression)!='{') --- */
        /* ---
           convert \font{abc} --> {\font~abc}
           ---------------------------------- */
        /* --- parse for {fontchars} arg, and bump expression past it --- */
        *expression =
            texsubexpr(*expression, fontchars, 0, "{", "}", 0, 0);
        if (msgfp != NULL && msglevel >= 99)
            fprintf(msgfp, "rastfont> \\%s fontchars=\"%s\"\n", name,
                    fontchars);
        /* --- convert all fontchars at the same time --- */
        strcpy(subexpr, "{");   /* start off with opening { */
        strcat(subexpr, name);  /* followed by font name */
        strcat(subexpr, "~");   /* followed by whitespace */
        strcat(subexpr, fontchars);     /* followed by all the chars */
        strcat(subexpr, "}");   /* terminate with closing } */
    } else {                    /* character-by-character */
        /* ---
           convert ABC to \calA\calB\calC
           ------------------------------ */
        int isprevchar = 0;     /* true if prev char converted */
        /* --- parse for {fontchars} arg, and bump expression past it --- */
        *expression =
            texsubexpr(*expression, fontchars, 0, "{", "}", 0, 0);
        if (msgfp != NULL && msglevel >= 99)
            fprintf(msgfp, "rastfont> \\%s fontchars=\"%s\"\n", name,
                    fontchars);
        /* --- convert fontchars one at a time --- */
        strcpy(subexpr, "{\\rm~");      /* start off with opening {\rm */
        strcpy(subexpr, "{");   /* nope, just start off with { */
        for (pfchars = fontchars; (fchar = *pfchars) != '\0'; pfchars++) {
            if (isthischar(fchar, WHITEMATH)) { /* some whitespace */
                if (0 || istext)        /* and we're in a text mode font */
                    strcat(subexpr, "\\;");
            } /* so respect whitespace */
            else {              /* char to be displayed in font */
                int exprlen = 0;        /* #chars in subexpr before fchar */
                int isinclass = 0;      /* set true if fchar in font class */
                /* --- class: 1=upper, 2=alpha, 3=alnum, 4=lower, 5=digit, 9=all --- */
                switch (class) {        /* check if fchar is in font class */
                default:
                    break;      /* no chars in unrecognized class */
                case 1:
                    if (isupper((int) fchar)) {
                        isinclass = 1;
                    }
                    break;
                case 2:
                    if (isalpha((int) fchar)) {
                        isinclass = 1;
                    }
                    break;
                case 3:
                    if (isalnum((int) fchar)) {
                        isinclass = 1;
                    }
                    break;
                case 4:
                    if (islower((int) fchar)) {
                        isinclass = 1;
                    }
                    break;
                case 5:
                    if (isdigit((int) fchar)) {
                        isinclass = 1;
                    }
                    break;
                case 9:
                    isinclass = 1;
                    break;
                }
                if (isinclass) {        /* convert current char to \font */
                    strcat(subexpr, name);      /* by prefixing it with font name */
                    isprevchar = 1;
                } /* and set flag to signal separator */
                else {          /* current char not in \font */
                    if (isprevchar)     /* extra separator only after \font */
                        if (isalpha(fchar))     /* separator only before alpha */
                            strcat(subexpr, "~");       /* need separator after \font */
                    isprevchar = 0;
                }               /* reset flag for next char */
                exprlen = strlen(subexpr);      /* #chars so far */
                subexpr[exprlen] = fchar;       /*fchar immediately after \fontname */
                subexpr[exprlen + 1] = '\0';
            }                   /* replace terminating '\0' */
        }                       /* --- end-of-for(pfchars) --- */
        strcat(subexpr, "}");   /* add closing } */
    }                           /* --- end-of-if/else(class<0) --- */
/* -------------------------------------------------------------------------
rasterize subexpression containing chars to be rendered at font
-------------------------------------------------------------------------- */
    if (msgfp != NULL && msglevel >= 99)
        fprintf(msgfp, "rastfont> subexpr=\"%s\"\n", subexpr);
    if ((fontsp = rasterize(subexpr, size))     /* rasterize chars in font */
        == NULL)
        goto end_of_job;        /* and quit if failed */
/* -------------------------------------------------------------------------
back to caller with chars rendered in font
-------------------------------------------------------------------------- */
  end_of_job:
    smashmargin = oldsmashmargin;       /* restore smash */
    mathsmashmargin = SMASHMARGIN;      /* this one probably not necessary */
    if (istext && fontsp != NULL)       /* raster contains text mode font */
        fontsp->type = blanksignal;     /* signal nosmash */
    return (fontsp);            /* chars rendered in font */
}                               /* --- end-of-function rastfont() --- */


/* ==========================================================================
 * Function:    rastbegin ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \begin{}...\end{}  handler, returns a subraster corresponding
 *      to array expression within environment, i.e., rewrites
 *      \begin{}...\end{} as mimeTeX equivalent, and rasterizes that.
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \begin to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \begin
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to array
 *              expression, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *rastbegin(const char **expression, int size,
                            subraster * basesp, int arg1, int arg2,
                            int arg3) {
    char subexpr[MAXSUBXSZ + 1];        /* \begin{} environment params */
    char *exprptr = NULL;
    char *begptr = NULL;
    char *endptr = NULL;
    char *braceptr = NULL;      /* ptrs */
    const char *begtoken = "\\begin{";
    const char *endtoken = "\\end{";    /*tokens we're looking for */
    char *delims = NULL;        /* mdelims[ienviron] */
    subraster *sp = NULL;       /* rasterize environment */
    int ienviron = 0;           /* environs[] index */
    int nbegins = 0;            /* #\begins nested beneath this one */
    int envlen = 0, sublen = 0; /* #chars in environ, subexpr */
    static int blevel = 0;      /* \begin...\end nesting level */
    static const char *mdelims[] = {
        NULL, NULL, NULL, NULL, "()", "[]", "{}", "||", "==",   /* for pbBvVmatrix */
    NULL, NULL, NULL, NULL, "{.", NULL, NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            NULL, NULL, NULL};
    static const char *environs[] = {   /* types of environments we process */
        "eqnarray",             /* 0 eqnarray environment */
            "array",            /* 1 array environment */
            "matrix",           /* 2 array environment */
            "tabular",          /* 3 array environment */
            "pmatrix",          /* 4 ( ) */
            "bmatrix",          /* 5 [ ] */
            "Bmatrix",          /* 6 { } */
            "vmatrix",          /* 7 | | */
            "Vmatrix",          /* 8 || || */
            "gather",           /* 9 gather environment */
            "align",            /* 10 align environment */
            "verbatim",         /* 11 verbatim environment */
            "picture",          /* 12 picture environment */
            "cases",            /* 13 cases environment */
            "equation",         /* 14 for \begin{equation} */
    NULL};                      /* trailer */
/* -------------------------------------------------------------------------
determine type of environment we're beginning
-------------------------------------------------------------------------- */
/* --- first bump nesting level --- */
    blevel++;                   /* count \begin...\begin...'s */
/* --- \begin must be followed by {type_of_environment} --- */
    exprptr = texsubexpr(*expression, subexpr, 0, "{", "}", 0, 0);
    if (*subexpr == '\0')
        goto end_of_job;        /* no environment given */
    while ((delims = strchr(subexpr, '*')) != NULL) {   /* have environment* */
        strsqueeze(delims, 1);
    }                           /* treat it as environment */
/* --- look up environment in our table --- */
    for (ienviron = 0;; ienviron++)     /* search table till NULL */
        if (environs[ienviron] == NULL) /* found NULL before match */
            goto end_of_job;    /* so quit */
        else /* see if we have an exact match */ if (memcmp(environs[ienviron], subexpr, strlen(subexpr)) == 0) /*match */
            break;              /* leave loop with ienviron index */
/* --- accumulate any additional params for this environment --- */
    *subexpr = '\0';            /* reset subexpr to empty string */
    // XXX:
    delims = (char *) mdelims[ienviron];        /* mdelims[] string for ienviron */
    if (delims != NULL) {       /* add appropriate opening delim */
        strcpy(subexpr, "\\");  /* start with \ for (,[,{,|,= */
        strcat(subexpr, delims);        /* then add opening delim */
        subexpr[2] = '\0';
    }                           /* remove extraneous closing delim */
    switch (ienviron) {
    default:
        goto end_of_job;        /* environ not implemented yet */
    case 0:                    /* \begin{eqnarray} */
        strcpy(subexpr, "\\array{rcl$");        /* set default rcl for eqnarray */
        break;
    case 1:
    case 2:
    case 3:                    /* \begin{array} followed by {lcr} */
        strcpy(subexpr, "\\array{");    /*start with mimeTeX \array{ command */
        skipwhite(exprptr);     /* bump to next non-white char */
        if (*exprptr == '{') {  /* assume we have {lcr} argument */
            exprptr = texsubexpr(exprptr, subexpr + 7, 0, "{", "}", 0, 0);      /*add on lcr */
            if (*(subexpr + 7) == '\0')
                goto end_of_job;        /* quit if no lcr */
            strcat(subexpr, "$");
        }                       /* add terminating $ to lcr */
        break;
    case 4:
    case 5:
    case 6:                    /* \begin{pmatrix} or b,B,v,Vmatrix */
    case 7:
    case 8:
        strcat(subexpr, "\\array{");    /*start with mimeTeX \array{ command */
        break;
    case 9:                    /* gather */
        strcat(subexpr, "\\array{c$");  /* center equations */
        break;
    case 10:                   /* align */
        strcat(subexpr, "\\array{rclrclrclrclrclrcl$"); /* a&=b & c&=d & etc */
        break;
    case 11:                   /* verbatim */
        strcat(subexpr, "{\\rm ");      /* {\rm ...} */
        /*strcat(subexpr,"\\\\{\\rm "); *//* \\{\rm } doesn't work in context */
        break;
    case 12:                   /* picture */
        strcat(subexpr, "\\picture");   /* picture environment */
        skipwhite(exprptr);     /* bump to next non-white char */
        if (*exprptr == '(') {  /*assume we have (width,height) arg */
            exprptr = texsubexpr(exprptr, subexpr + 8, 0, "(", ")", 0, 1);      /*add on arg */
            if (*(subexpr + 8) == '\0')
                goto end_of_job;
        }                       /* quit if no arg */
        strcat(subexpr, "{");   /* opening {  after (width,height) */
        break;
    case 13:                   /* cases */
        strcat(subexpr, "\\array{ll$"); /* a&b \\ c&d etc */
        break;
    case 14:                   /* \begin{equation} */
        strcat(subexpr, "{");   /* just enclose expression in {}'s */
        break;
    }                           /* --- end-of-switch(ienviron) --- */
/* -------------------------------------------------------------------------
locate matching \end{...}
-------------------------------------------------------------------------- */
/* --- first \end following \begin --- */
    if ((endptr = strstr(exprptr, endtoken))    /* find 1st \end following \begin */
        == NULL)
        goto end_of_job;        /* and quit if no \end found */
/* --- find matching endptr by pushing past any nested \begin's --- */
    begptr = exprptr;           /* start after first \begin{...} */
    while (1) {                 /*break when we find matching \end */
        /* --- first, set ptr to closing } terminating current \end{...} --- */
        if ((braceptr = strchr(endptr + 1, '}'))        /* find 1st } following \end{ */
            == NULL)
            goto end_of_job;    /* and quit if no } found */
        /* -- locate next nested \begin --- */
        if ((begptr = strstr(begptr, begtoken)) /* find next \begin{...} */
            == NULL)
            break;              /*no more, so we have matching \end */
        begptr += strlen(begtoken);     /* push ptr past token */
        if (begptr >= endptr)
            break;              /* past endptr, so not nested */
        /* --- have nested \begin, so push forward to next \end --- */
        nbegins++;              /* count another nested \begin */
        if ((endptr = strstr(endptr + strlen(endtoken), endtoken))      /* find next \end */
            == NULL)
            goto end_of_job;    /* and quit if none found */
    }                           /* --- end-of-while(1) --- */
/* --- push expression past closing } of \end{} --- */
    *expression = braceptr + 1; /* resume processing after } */
/* -------------------------------------------------------------------------
add on everything (i.e., the ...'s) between \begin{}[{}] ... \end{}
-------------------------------------------------------------------------- */
/* --- add on everything, completing subexpr for \begin{}...\end{} --- */
    sublen = strlen(subexpr);   /* #chars in "preamble" */
    envlen = (int) (endptr - exprptr);  /* #chars between \begin{}{}...\end */
    memcpy(subexpr + sublen, exprptr, envlen);  /*concatanate environ after subexpr */
    subexpr[sublen + envlen] = '\0';    /* and null-terminate */
    if (2 > 1)                  /* always... */
        strcat(subexpr, "}");   /* ...followed by terminating } */
/* --- add terminating \right), etc, if necessary --- */
    if (delims != (char *) NULL) {      /* need closing delim */
        strcat(subexpr, "\\");  /* start with \ for ),],},|,= */
        strcat(subexpr, delims + 1);
    }
    /* add appropriate closing delim */
    /* -------------------------------------------------------------------------
       change nested \begin...\end to {\begin...\end} so \array{} can handle them
       -------------------------------------------------------------------------- */
    if (nbegins > 0)            /* have nested begins */
        if (blevel < 2) {       /* only need to do this once */
            begptr = subexpr;   /* start at beginning of subexpr */
            while ((begptr = strstr(begptr, begtoken)) != NULL) {       /* have \begin{...} */
                strchange(0, begptr, "{");      /* \begin --> {\begin */
                begptr += strlen(begtoken);
            }                   /* continue past {\begin */
            endptr = subexpr;   /* start at beginning of subexpr */
            while ((endptr = strstr(endptr, endtoken)) != NULL) /* have \end{...} */
                if ((braceptr = strchr(endptr + 1, '}'))        /* find 1st } following \end{ */
                    == NULL)
                    goto end_of_job;    /* and quit if no } found */
                else {          /* found terminating } */
                    strchange(0, braceptr, "}");        /* \end{...} --> \end{...}} */
                    endptr = braceptr + 1;
                }               /* continue past \end{...} */
        }
    /* --- end-of-if(nbegins>0) --- */
    /* -------------------------------------------------------------------------
       post process as necessary
       -------------------------------------------------------------------------- */
    switch (ienviron) {
    default:
        break;                  /* no post-processing required */
    case 10:                   /* align */
        strreplace(subexpr, "&=", "#*@*#=", 0); /* tag all &='s */
        strreplace(subexpr, "&<", "#*@*#<", 0); /* tag all &<'s */
        strreplace(subexpr, "&\\lt", "#*@*#<", 0);      /* tag all &\lt's */
        strreplace(subexpr, "&\\leq", "#*@*#\\leq", 0); /* tag all &\leq's */
        strreplace(subexpr, "&>", "#*@*#>", 0); /* tag all &>'s */
        strreplace(subexpr, "&\\gt", "#*@*#>", 0);      /* tag all &\gt's */
        strreplace(subexpr, "&\\geq", "#*@*#\\geq", 0); /* tag all &\geq's */
        if (nbegins < 1)        /* don't modify nested arrays */
            strreplace(subexpr, "&", "\\hspace{10}&\\hspace{10}", 0);   /* add space */
        strreplace(subexpr, "#*@*#=", "& = &", 0);      /*restore and xlate tagged &='s */
        strreplace(subexpr, "#*@*#<", "& \\lt &", 0);   /*restore, xlate tagged &<'s */
        strreplace(subexpr, "#*@*#\\leq", "& \\leq &", 0);      /*xlate tagged &\leq's */
        strreplace(subexpr, "#*@*#>", "& \\gt &", 0);   /*restore, xlate tagged &>'s */
        strreplace(subexpr, "#*@*#\\geq", "& \\geq &", 0);      /*xlate tagged &\geq's */
        break;
    case 11:                   /* verbatim */
        strreplace(subexpr, "\n", "\\\\", 0);   /* xlate \n newline to latex \\ */
        /*strcat(subexpr,"\\\\"); *//* add final latex \\ newline */
        break;
    case 12:                   /* picture */
        strreplace(subexpr, "\\put ", " ", 0);  /*remove \put's (not really needed) */
        strreplace(subexpr, "\\put(", "(", 0);  /*remove \put's (not really needed) */
        strreplace(subexpr, "\\oval", "\\circle", 0);   /* actually an ellipse */
        break;
    }                           /* --- end-of-switch(ienviron) --- */
/* -------------------------------------------------------------------------
return rasterized mimeTeX equivalent of \begin{}...\end{} environment
-------------------------------------------------------------------------- */
/* --- debugging output --- */
    if (msgfp != NULL && msglevel >= 99)
        fprintf(msgfp, "rastbegin> subexpr=%s\n", subexpr);
/* --- rasterize mimeTeX equivalent of \begin{}...\end{} environment --- */
    sp = rasterize(subexpr, size);      /* rasterize subexpr */
  end_of_job:
    blevel--;                   /* decrement \begin nesting level */
    return (sp);                /* back to caller with sp or NULL */
}                               /* --- end-of-function rastbegin() --- */


/* ==========================================================================
 * Function:    rastarray ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \array handler, returns a subraster corresponding to array
 *      expression (immediately following \array) at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \array to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \array
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to array
 *              expression, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *          \array{3,lcrBC$a&b&c\\d&e&f\\etc}
 *        o The 3,lcrBC$ part is an optional "preamble".  The lcr means
 *      what you think, i.e., "horizontal" left,center,right
 *      justification down corresponding column.  The new BC means
 *      "vertical" baseline,center justification across corresponding
 *      row.  The leading 3 specifies the font size 0-4 to be used.
 *      You may also specify +1,-1,+2,-2, etc, which is used as an
 *      increment to the current font size, e.g., -1,lcr$ uses
 *      one font size smaller than current.  Without a leading
 *      + or -, the font size is "absolute".
 *        o The preamble can also be just lcrBC$ without a leading
 *      size-part, or just 3$ without a trailing lcrBC-part.
 *      The default size is whatever is current, and the
 *      default justification is c(entered) and B(aseline).
 * ======================================================================= */
static subraster *rastarray(const char **expression, int size,
                            subraster * basesp, int arg1, int arg2,
                            int arg3) {
    char subexpr[MAXSUBXSZ + 1];
    char *exprptr;              /*parse array subexpr */
    char subtok[MAXTOKNSZ + 1];
    /* not used --char *subptr = subtok; *//* &,\\ inside { } not a delim */
    char token[MAXTOKNSZ + 1];
    char *tokptr = token;       /* subexpr token to rasterize */
    char *preptr = token;       /*process optional size,lcr preamble */
    const char *coldelim = "&";
    const char *rowdelim = "\\";        /* need escaped rowdelim */
    int maxarraysz = 63;        /* max #rows, cols */
    int justify[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /* -1,0,+1 = l,c,r */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    int hline[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /* hline above row? */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    int vline[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /*vline left of col? */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    int colwidth[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /*widest tokn in col */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    int rowheight[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /* "highest" in row */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    int fixcolsize[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /*1=fixed col width */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    int fixrowsize[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /*1=fixed row height */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    int rowbaseln[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /* baseline for row */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    int vrowspace[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /*extra //[len]space */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    int rowcenter[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /*true = vcenter row */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    static int                  /* --- propagate global values across arrays --- */
     gjustify[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /* -1,0,+1 = l,c,r */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    static int gcolwidth[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /*widest tokn in col */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    static int growheight[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /* "highest" in row */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    static int gfixcolsize[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /*1=fixed col width */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    static int gfixrowsize[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /*1=fixed row height */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    static int growcenter[65] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /*true = vcenter row */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};
    int rowglobal = 0, colglobal = 0,   /* true to set global values */
        rowpropagate = 0, colpropagate = 0;     /* true if propagating values */
    int irow, nrows = 0, icol, ncols[65],       /*#rows in array, #cols in each row */
        maxcols = 0;            /* max# cols in any single row */
    int itoken, ntokens = 0,    /* index, total #tokens in array */
        subtoklen = 0,          /* strlen of {...} subtoken */
        istokwhite = 1,         /* true if token all whitespace */
        nnonwhite = 0;          /* #non-white tokens */
    int isescape = 0, wasescape = 0,    /* current,prev chars escape? */
        ischarescaped = 0,      /* is current char escaped? */
        nescapes = 0;           /* #consecutive escapes */
    subraster *rasterize(), *toksp[1025],       /* rasterize tokens */
    *new_subraster(), *arraysp = NULL;  /* subraster for entire array */
    raster *arrayrp = NULL;     /* raster for entire array */
    int delete_subraster();     /* free toksp[] workspace at eoj */
    int rowspace = 2, colspace = 4,     /* blank space between rows, cols */
        hspace = 1, vspace = 1; /*space to accommodate hline,vline */
    int width = 0, height = 0,  /* width,height of array */
        leftcol = 0, toprow = 0;        /*upper-left corner for cell in it */
    int rastput();              /* embed tokens/cells in array */
    int rule_raster();          /* draw hlines and vlines in array */
    const char *hlchar = "\\hline";
    const char *hdchar = "\\hdash";     /* token signals hline */
    char *texchar(), hltoken[1025];     /* extract \hline from token */
    int ishonly = 0, hltoklen, minhltoklen = 3; /*flag, token must be \hl or \hd */
    int isnewrow = 1;           /* true for new row */
    int pixsz = 1;              /*default #bits per pixel, 1=bitmap */
    int evalterm(), evalue = 0; /* evaluate [arg], {arg} */
    static int mydaemonlevel = 0;       /* check against global daemonlevel */
/* -------------------------------------------------------------------------
Macros to determine extra raster space required for vline/hline
-------------------------------------------------------------------------- */
#define vlinespace(icol) \
    ( vline[icol] == 0?  0 :    /* no vline so no space needed */   \
      ( icol<1 || icol>=maxcols? vspace+(colspace+1)/2 : vspace ) )
#define hlinespace(irow) \
    ( hline[irow] == 0?  0 :    /* no hline so no space needed */   \
      ( irow<1 || irow>=nrows? hspace+(rowspace+1)/2 : hspace ) )
/* -------------------------------------------------------------------------
Obtain array subexpression
-------------------------------------------------------------------------- */
/* --- parse for array subexpression, and bump expression past it --- */
    subexpr[1] = *subexpr = ' ';        /* set two leading blanks */
    *expression = texsubexpr(*expression, subexpr + 2, 0, "{", "}", 0, 0);
    if (msglevel >= 29 && msgfp != NULL)        /* debugging, display array */
        fprintf(msgfp, "rastarray> %.256s\n", subexpr + 2);
    if (*(subexpr + 2) == '\0') /* couldn't get subexpression */
        goto end_of_job;        /* nothing to do, so quit */
/* -------------------------------------------------------------------------
reset static arrays if main re-entered as daemon (or dll)
-------------------------------------------------------------------------- */
    if (mydaemonlevel != daemonlevel) { /* main re-entered */
        for (icol = 0; icol <= maxarraysz; icol++)      /* for each array[] index */
            gjustify[icol] = gcolwidth[icol] = growheight[icol] =
                gfixcolsize[icol] = gfixrowsize[icol] = growcenter[icol] =
                0;
        mydaemonlevel = daemonlevel;
    }

    /* update mydaemonlevel */
    /* -------------------------------------------------------------------------
       process optional size,lcr preamble if present
       -------------------------------------------------------------------------- */
    /* --- reset size, get lcr's, and push exprptr past preamble --- */
    exprptr = preamble(subexpr + 2, &size, preptr);     /* reset size and get lcr's */
/* --- init with global values --- */
    for (icol = 0; icol <= maxarraysz; icol++) {        /* propagate global values... */
        justify[icol] = gjustify[icol]; /* -1,0,+1 = l,c,r */
        colwidth[icol] = gcolwidth[icol];       /* column width */
        rowheight[icol] = growheight[icol];     /* row height */
        fixcolsize[icol] = gfixcolsize[icol];   /* 1=fixed col width */
        fixrowsize[icol] = gfixrowsize[icol];   /* 1=fixed row height */
        rowcenter[icol] = growcenter[icol];
    }                           /* true = vcenter row */
/* --- process lcr's, etc in preamble --- */
    itoken = 0;                 /* debugging flag */
    if (msglevel >= 29 && msgfp != NULL)        /* debugging, display preamble */
        if (*preptr != '\0')    /* if we have one */
            fprintf(msgfp,
                    "rastarray> preamble= \"%.256s\"\nrastarray> preamble: ",
                    preptr);
    irow = icol = 0;            /* init lcr counts */
    while (*preptr != '\0') {   /* check preamble text for lcr */
        char prepchar = *preptr;        /* current preamble character */
        int prepcase = (islower(prepchar) ? 1 : (isupper(prepchar) ? 2 : 0));   /*1,2,or 0 */
        if (irow < maxarraysz && icol < maxarraysz)
            switch ( /*tolower */ (prepchar)) {
            default:
                break;          /* just flush unrecognized chars */
            case 'l':
                justify[icol] = (-1);   /*left-justify this column */
                if (colglobal)
                    gjustify[irow] = justify[irow];
                break;
            case 'c':
                justify[icol] = (0);    /* center this column */
                if (colglobal)
                    gjustify[irow] = justify[irow];
                break;
            case 'r':
                justify[icol] = (+1);   /* right-justify this col */
                if (colglobal)
                    gjustify[irow] = justify[irow];
                break;
            case '|':
                vline[icol] += 1;
                break;          /* solid vline left of col */
            case '.':
                vline[icol] = (-1);
                break;          /*dashed vline left of col */
            case 'b':
                prepchar = 'B';
                prepcase = 2;   /* alias for B */
            case 'B':
                break;          /* baseline-justify row */
            case 'v':
                prepchar = 'C';
                prepcase = 2;   /* alias for C */
            case 'C':
                rowcenter[irow] = 1;    /* vertically center row */
                if (rowglobal)
                    growcenter[irow] = rowcenter[irow];
                break;
            case 'g':
                colglobal = 1;
                prepcase = 0;
                break;          /* set global col values */
            case 'G':
                rowglobal = 1;
                prepcase = 0;
                break;          /* set global row values */
            case '#':
                colglobal = rowglobal = 1;
                break;
            }                   /* set global col,row vals */
        if (msglevel >= 29 && msgfp != NULL)    /* debugging */
            fprintf(msgfp, " %c[%d]", prepchar,
                    (prepcase ==
                     1 ? icol + 1 : (prepcase == 2 ? irow + 1 : 0)));
        preptr++;               /* check next char for lcr */
        itoken++;               /* #lcr's processed (debugging only) */
        /* --- check for number or +number specifying colwidth or rowheight --- */
        if (prepcase != 0) {    /* only check upper,lowercase */
            int ispropagate = (*preptr == '+' ? 1 : 0); /* leading + propagates width/ht */
            if (ispropagate) {  /* set row or col propagation */
                if (prepcase == 1)
                    colpropagate = 1;   /* propagating col values */
                else if (prepcase == 2)
                    rowpropagate = 1;
            }                   /*propagating row values */
            if (!colpropagate && prepcase == 1) {
                colwidth[icol] = 0;     /* reset colwidth */
                fixcolsize[icol] = 0;
            }                   /* reset width flag */
            if (!rowpropagate && prepcase == 2) {
                rowheight[irow] = 0;    /* reset row height */
                fixrowsize[irow] = 0;
            }                   /* reset height flag */
            if (ispropagate)
                preptr++;       /* bump past leading + */
            if (isdigit(*preptr)) {     /* digit follows character */
                char *endptr = NULL;    /* preptr set to 1st char after num */
                int size_digit = (int) (strtol(preptr, &endptr, 10));   /* interpret number */
                const char *whchars = "?wh";    /* debugging width/height labels */
                preptr = endptr;        /* skip over all digits */
                if (size_digit == 0 || (size_digit >= 3 && size_digit <= 500)) {        /* sanity check */
                    int index;  /* icol,irow...maxarraysz index */
                    if (prepcase == 1)  /* lowercase signifies colwidth */
                        for (index = icol; index <= maxarraysz; index++) {      /*propagate col size */
                            colwidth[index] = size_digit;       /* set colwidth to fixed size */
                            fixcolsize[index] = (size_digit > 0 ? 1 : 0);       /* set fixed width flag */
                            justify[index] = justify[icol];     /* and propagate justification */
                            if (colglobal) {    /* set global values */
                                gcolwidth[index] = colwidth[index];     /* set global col width */
                                gfixcolsize[index] = fixcolsize[index]; /*set global width flag */
                                gjustify[index] = justify[icol];
                            }   /* set global col justify */
                            if (!ispropagate)
                                break;
                        } /* don't propagate */
                    else        /* uppercase signifies rowheight */
                        for (index = irow; index <= maxarraysz; index++) {      /*propagate row size */
                            rowheight[index] = size_digit;      /* set rowheight to size */
                            fixrowsize[index] = (size_digit > 0 ? 1 : 0);       /* set fixed height flag */
                            rowcenter[index] = rowcenter[irow]; /* and propagate row center */
                            if (rowglobal) {    /* set global values */
                                growheight[index] = rowheight[index];   /* set global row height */
                                gfixrowsize[index] = fixrowsize[index]; /*set global height flag */
                                growcenter[index] = rowcenter[irow];
                            }   /*set global row center */
                            if (!ispropagate)
                                break;
                        }       /* don't propagate */
                }               /* --- end-of-if(size>=3&&size<=500) --- */
                if (msglevel >= 29 && msgfp != NULL)    /* debugging */
                    fprintf(msgfp, ":%c=%d/fix#%d", whchars[prepcase],
                            (prepcase ==
                             1 ? colwidth[icol] : rowheight[irow]),
                            (prepcase ==
                             1 ? fixcolsize[icol] : fixrowsize[irow]));
            }                   /* --- end-of-if(isdigit()) --- */
        }                       /* --- end-of-if(prepcase!=0) --- */
        if (prepcase == 1)
            icol++;             /* bump col if lowercase lcr */
        else if (prepcase == 2)
            irow++;             /* bump row if uppercase BC */
    }                           /* --- end-of-while(*preptr!='\0') --- */
    if (msglevel >= 29 && msgfp != NULL)        /* debugging, emit final newline */
        if (itoken > 0)         /* if we have preamble */
            fprintf(msgfp, "\n");
/* -------------------------------------------------------------------------
tokenize and rasterize components  a & b \\ c & d \\ etc  of subexpr
-------------------------------------------------------------------------- */
/* --- rasterize tokens one at a time, and maintain row,col counts --- */
    nrows = 0;                  /* start with top row */
    ncols[nrows] = 0;           /* no tokens/cols in top row yet */
    while (1) {                 /* scan chars till end */
        /* --- local control flags --- */
        int iseox = (*exprptr == '\0'), /* null signals end-of-expression */
            iseor = iseox,      /* \\ or eox signals end-of-row */
            iseoc = iseor;      /* & or eor signals end-of-col */
        /* --- check for escapes --- */
        isescape = isthischar(*exprptr, ESCAPE);        /* is current char escape? */
        wasescape = (!isnewrow && isthischar(*(exprptr - 1), ESCAPE));  /*prev char esc? */
        nescapes = (wasescape ? nescapes + 1 : 0);      /* # preceding consecutive escapes */
        ischarescaped = (nescapes % 2 == 0 ? 0 : 1);    /* is current char escaped? */
        /* -----------------------------------------------------------------------
           check for {...} subexpression starting from where we are now
           ------------------------------------------------------------------------ */
        if (*exprptr == '{'     /* start of {...} subexpression */
            && !ischarescaped) {        /* if not escaped \{ */
                                                                                                /* not used --subptr = */ texsubexpr(exprptr, subtok, 4095, "{", "}", 1, 1);
                                                                                                /*entire subexpr */
            subtoklen = strlen(subtok); /* #chars in {...} */
            memcpy(tokptr, exprptr, subtoklen); /* copy {...} to accumulated token */
            tokptr += subtoklen;        /* bump tokptr to end of token */
            exprptr += subtoklen;       /* and bump exprptr past {...} */
            istokwhite = 0;     /* signal non-empty token */
            continue;           /* continue with char after {...} */
        }

        /* --- end-of-if(*exprptr=='{') --- */
        /* -----------------------------------------------------------------------
           check for end-of-row(\\) and/or end-of-col(&)
           ------------------------------------------------------------------------ */
        /* --- check for (escaped) end-of-row delimiter --- */
        if (isescape && !ischarescaped) /* current char is escaped */
            if (isthischar(*(exprptr + 1), rowdelim)    /* next char is rowdelim */
                ||*(exprptr + 1) == '\0') {     /* or a pathological null */
                iseor = 1;      /* so set end-of-row flag */
                wasescape = isescape = nescapes = 0;
            }
        /* reset flags for new row */
        /* --- check for end-of-col delimiter --- */
        if (iseor               /* end-of-row signals end-of-col */
            || (!ischarescaped && isthischar(*exprptr, coldelim)))      /*or unescaped coldel */
            iseoc = 1;          /* so set end-of-col flag */
        /* -----------------------------------------------------------------------
           rasterize completed token
           ------------------------------------------------------------------------ */
        if (iseoc) {            /* we have a completed token */
            *tokptr = '\0';     /* first, null-terminate token */
            /* --- check first token in row for [len] and/or \hline or \hdash --- */
            ishonly = 0;        /*init for token not only an \hline */
            if (ncols[nrows] == 0) {    /*\hline must be first token in row */
                tokptr = token;
                skipwhite(tokptr);      /* skip whitespace after // */
                /* --- first check for optional [len] --- */
                if (*tokptr == '[') {   /* have [len] if leading char is [ */
                    /* ---parse [len] and bump tokptr past it, interpret as double--- */
                    char lenexpr[128];
                    int len;    /* chars between [...] as int */
                    tokptr =
                        texsubexpr(tokptr, lenexpr, 127, "[", "]", 0, 0);
                    if (*lenexpr != '\0') {     /* got [len] expression */
                        evalue = evalterm(mimestore, lenexpr);  /* evaluate len expression */
                        len = iround(unitlength * ((double) evalue));   /* len in pixels */
                        if (len >= (-63) && len <= 255) {       /* sanity check */
                            vrowspace[nrows] = len;     /* extra vspace before this row */
                            strsqueezep(token, tokptr); /* flush [len] from token */
                            tokptr = token;
                            skipwhite(tokptr);
                        }
                    }           /* reset ptr, skip white */
                }
                /* --- end-of-if(*tokptr=='[') --- */
                /* --- now check for \hline or \hdash --- */
                tokptr = texchar(tokptr, hltoken);      /* extract first char from token */
                hltoklen = strlen(hltoken);     /* length of first char */
                if (hltoklen >= minhltoklen) {  /*token must be at least \hl or \hd */
                    if (memcmp(hlchar, hltoken, hltoklen) == 0) /* we have an \hline */
                        hline[nrows] += 1;      /* bump \hline count for row */
                    else if (memcmp(hdchar, hltoken, hltoklen) == 0)    /*we have an \hdash */
                        hline[nrows] = (-1);
                }               /* set \hdash flag for row */
                if (hline[nrows] != 0) {        /* \hline or \hdash prefixes token */
                    skipwhite(tokptr);  /* flush whitespace after \hline */
                    if (*tokptr == '\0' /* end-of-expression after \hline */
                        || isthischar(*tokptr, coldelim)) {     /* or unescaped coldelim */
                        istokwhite = 1; /* so token contains \hline only */
                        if (iseox)
                            ishonly = 1;
                    } /* ignore entire row at eox */
                    else {      /* token contains more than \hline */
                        strsqueezep(token, tokptr);
                    }
                }               /* so flush \hline */
            }
            /* --- end-of-if(ncols[nrows]==0) --- */
            /* --- rasterize completed token --- */
            toksp[ntokens] = (istokwhite ? NULL :       /* don't rasterize empty token */
                              rasterize(token, size));  /* rasterize non-empty token */
            if (toksp[ntokens] != NULL) /* have a rasterized token */
                nnonwhite++;    /* bump rasterized token count */
            /* --- maintain colwidth[], rowheight[] max, and rowbaseln[] --- */
            if (toksp[ntokens] != NULL) {       /* we have a rasterized token */
                /* --- update max token "height" in current row, and baseline --- */
                int twidth = ((toksp[ntokens])->image)->width,  /* width of token */
                    theight = ((toksp[ntokens])->image)->height,        /* height of token */
                    tbaseln = (toksp[ntokens])->baseline,       /* baseline of token */
                    rheight = rowheight[nrows], /* current max height for row */
                    rbaseln = rowbaseln[nrows]; /* current baseline for max height */
                if (0 || fixrowsize[nrows] == 0)        /* rowheight not fixed */
                    rowheight[nrows] = /*max2( rheight, */ (    /* current (max) rowheight */
                                                               max2(rbaseln + 1, tbaseln + 1)   /* max height above baseline */
                                                               +max2(rheight - rbaseln - 1, theight - tbaseln - 1));    /* plus max below */
                rowbaseln[nrows] = max2(rbaseln, tbaseln);      /*max space above baseline */
                /* --- update max token width in current column --- */
                icol = ncols[nrows];    /* current column index */
                if (0 || fixcolsize[icol] == 0) /* colwidth not fixed */
                    colwidth[icol] = max2(colwidth[icol], twidth);      /*widest token in col */
            }
            /* --- end-of-if(toksp[]!=NULL) --- */
            /* --- bump counters --- */
            if (!ishonly)       /* don't count only an \hline */
                if (ncols[nrows] < maxarraysz) {        /* don't overflow arrays */
                    ntokens++;  /* bump total token count */
                    ncols[nrows] += 1;
                }
            /* and bump #cols in current row */
            /* --- get ready for next token --- */
            tokptr = token;     /* reset ptr for next token */
            istokwhite = 1;     /* next token starts all white */
        }
        /* --- end-of-if(iseoc) --- */
        /* -----------------------------------------------------------------------
           bump row as necessary
           ------------------------------------------------------------------------ */
        if (iseor) {            /* we have a completed row */
            maxcols = max2(maxcols, ncols[nrows]);      /* max# cols in array */
            if (ncols[nrows] > 0 || hline[nrows] == 0)  /*ignore row with only \hline */
                if (nrows < maxarraysz) /* don't overflow arrays */
                    nrows++;    /* bump row count */
            ncols[nrows] = 0;   /* no cols in this row yet */
            if (!iseox) {       /* don't have a null yet */
                exprptr++;      /* bump past extra \ in \\ delim */
                iseox = (*exprptr == '\0');
            }                   /* recheck for pathological \null */
            isnewrow = 1;       /* signal start of new row */
        } /* --- end-of-if(iseor) --- */
        else
            isnewrow = 0;       /* no longer first col of new row */
        /* -----------------------------------------------------------------------
           quit when done, or accumulate char in token and proceed to next char
           ------------------------------------------------------------------------ */
        /* --- quit when done --- */
        if (iseox)
            break;              /* null terminator signalled done */
        /* --- accumulate chars in token --- */
        if (!iseoc) {           /* don't accumulate delimiters */
            *tokptr++ = *exprptr;       /* accumulate non-delim char */
            if (!isthischar(*exprptr, WHITESPACE))      /* this token isn't empty */
                istokwhite = 0;
        }
        /* so reset flag to rasterize it */
        /* --- ready for next char --- */
        exprptr++;              /* bump ptr */
    }                           /* --- end-of-while(*exprptr!='\0') --- */
/* --- make sure we got something to do --- */
    if (nnonwhite < 1)          /* completely empty array */
        goto end_of_job;        /* NULL back to caller */
/* -------------------------------------------------------------------------
determine dimensions of array raster and allocate it
-------------------------------------------------------------------------- */
/* --- adjust colspace --- */
    colspace = 2 + 2 * size;    /* temp kludge */
/* --- reset propagated sizes at boundaries of array --- */
    colwidth[maxcols] = rowheight[nrows] = 0;   /* reset explicit 0's at edges */
/* --- determine width of array raster --- */
    width = colspace * (maxcols - 1);   /* empty space between cols */
    if (msglevel >= 29 && msgfp != NULL)        /* debugging */
        fprintf(msgfp, "rastarray> %d cols,  widths: ", maxcols);
    for (icol = 0; icol <= maxcols; icol++) {   /* and for each col */
        width += colwidth[icol];        /*width of this col (0 for maxcols) */
        width += vlinespace(icol);      /*plus space for vline, if present */
        if (msglevel >= 29 && msgfp != NULL)    /* debugging */
            fprintf(msgfp, " %d=%2d+%d", icol + 1, colwidth[icol],
                    (vlinespace(icol)));
    }
/* --- determine height of array raster --- */
    height = rowspace * (nrows - 1);    /* empty space between rows */
    if (msglevel >= 29 && msgfp != NULL)        /* debugging */
        fprintf(msgfp, "\nrastarray> %d rows, heights: ", nrows);
    for (irow = 0; irow <= nrows; irow++) {     /* and for each row */
        height += rowheight[irow];      /*height of this row (0 for nrows) */
        height += vrowspace[irow];      /*plus extra //[len], if present */
        height += hlinespace(irow);     /*plus space for hline, if present */
        if (msglevel >= 29 && msgfp != NULL)    /* debugging */
            fprintf(msgfp, " %d=%2d+%d", irow + 1, rowheight[irow],
                    (hlinespace(irow)));
    }
/* --- allocate subraster and raster for array --- */
    if (msglevel >= 29 && msgfp != NULL)        /* debugging */
        fprintf(msgfp,
                "\nrastarray> tot width=%d(colspc=%d) height=%d(rowspc=%d)\n",
                width, colspace, height, rowspace);
    if ((arraysp = new_subraster(width, height, pixsz)) /* allocate new subraster */
        == NULL)
        goto end_of_job;        /* quit if failed */
/* --- initialize subraster parameters --- */
    arraysp->type = IMAGERASTER;        /* image */
    arraysp->symdef = NULL;     /* not applicable for image */
    arraysp->baseline = min2(height / 2 + 5, height - 1);       /*is a little above center good? */
    arraysp->size = size;       /* size (probably unneeded) */
    arrayrp = arraysp->image;   /* raster embedded in subraster */
/* -------------------------------------------------------------------------
embed tokens/cells in array
-------------------------------------------------------------------------- */
    itoken = 0;                 /* start with first token */
    toprow = 0;                 /* start at top row of array */
    for (irow = 0; irow <= nrows; irow++) {     /*tokens were accumulated row-wise */
        /* --- initialization for row --- */
        int baseline = rowbaseln[irow]; /* baseline for this row */
        if (hline[irow] != 0) { /* need hline above this row */
            int hrow = (irow < 1 ? 0 : toprow - rowspace / 2);  /* row for hline */
            if (irow >= nrows)
                hrow = height - 1;      /* row for bottom hline */
            rule_raster(arrayrp, hrow, 0, width, 1,
                        (hline[irow] < 0 ? 1 : 0));
        }                       /* hline */
        if (irow >= nrows)
            break;              /*just needed \hline for irow=nrows */
        toprow += vrowspace[irow];      /* extra //[len] space above irow */
        if (toprow < 0)
            toprow = 0;         /* check for large negative [-len] */
        toprow += hlinespace(irow);     /* space for hline above irow */
        leftcol = 0;            /* start at leftmost column */
        for (icol = 0; icol < ncols[irow]; icol++) {    /* go through cells in this row */
            subraster *tsp = toksp[itoken];     /* token that belongs in this cell */
            /* --- first adjust leftcol for vline to left of icol, if present ---- */
            leftcol += vlinespace(icol);        /* space for vline to left of col */
            /* --- now rasterize cell ---- */
            if (tsp != NULL) {  /* have a rasterized cell token */
                /* --- local parameters --- */
                int cwidth = colwidth[icol],    /* total column width */
                    twidth = (tsp->image)->width,       /* token width */
                    theight = (tsp->image)->height,     /* token height */
                    tokencol = 0,       /*H offset (init for left justify) */
                    tokenrow = baseline - tsp->baseline;        /*V offset (init for baseline) */
                /* --- adjust leftcol for vline to left of icol, if present ---- */
                /*leftcol += vlinespace(icol); *//* space for vline to left of col */
                /* --- reset justification (if not left-justified) --- */
                if (justify[icol] == 0) /* but user wants it centered */
                    tokencol = (cwidth - twidth + 1) / 2;       /* so split margin left/right */
                else if (justify[icol] == 1)    /* or user wants right-justify */
                    tokencol = cwidth - twidth; /* so put entire margin at left */
                /* --- reset vertical centering (if not baseline-aligned) --- */
                if (rowcenter[irow])    /* center cells in row vertically */
                    tokenrow = (rowheight[irow] - theight) / 2; /* center row */
                /* --- embed token raster at appropriate place in array raster --- */
                rastput(arrayrp, tsp->image,    /* overlay cell token in array */
                        toprow + tokenrow,      /*with aligned baseline or centered */
                        leftcol + tokencol, 1); /* and justified as requested */
            }                   /* --- end-of-if(tsp!=NULL) --- */
            itoken++;           /* bump index for next cell */
            leftcol += colwidth[icol] + colspace        /*move leftcol right for next col */
                /* + vlinespace(icol) */ ;
            /*don't add space for vline to left of col */
        }                       /* --- end-of-for(icol) --- */
        toprow += rowheight[irow] + rowspace;   /* move toprow down for next row */
    }                           /* --- end-of-for(irow) --- */
/* -------------------------------------------------------------------------
draw vlines as necessary
-------------------------------------------------------------------------- */
    leftcol = 0;                /* start at leftmost column */
    for (icol = 0; icol <= maxcols; icol++) {   /* check each col for a vline */
        if (vline[icol] != 0) { /* need vline to left of this col */
            int vcol = (icol < 1 ? 0 : leftcol - colspace / 2); /* column for vline */
            if (icol >= maxcols)
                vcol = width - 1;       /*column for right edge vline */
            rule_raster(arrayrp, 0, vcol, 1, height,
                        (vline[icol] < 0 ? 2 : 0));
        }                       /* vline */
        leftcol += vlinespace(icol);    /* space for vline to left of col */
        if (icol < maxcols)     /* don't address past end of array */
            leftcol += colwidth[icol] + colspace;       /*move leftcol right for next col */
    }                           /* --- end-of-for(icol) --- */
/* -------------------------------------------------------------------------
free workspace and return final result to caller
-------------------------------------------------------------------------- */
  end_of_job:
    /* --- free workspace --- */
    if (ntokens > 0)            /* if we have workspace to free */
        while (--ntokens >= 0)  /* free each token subraster */
            if (toksp[ntokens] != NULL) /* if we rasterized this cell */
                delete_subraster(toksp[ntokens]);       /* then free it */
    /* --- return final result to caller --- */
    return (arraysp);
}                               /* --- end-of-function rastarray() --- */


/* ==========================================================================
 * Function:    rastpicture ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \picture handler, returns subraster corresponding to picture
 *      expression (immediately following \picture) at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \picture to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \picture
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to picture
 *              expression, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \picture(width,height){(x,y){pic_elem}~(x,y){pic_elem}~etc}
 *        o 
 * ======================================================================= */
subraster *rastpicture(const char **expression, int size,
                       subraster * basesp, int arg1, int arg2, int arg3) {
    char picexpr[2049], *picptr = picexpr,      /* picture {expre} */
        putexpr[256], *putptr, *multptr,        /*[multi]put (x,y[;xinc,yinc;num]) */
        pream[96], *preptr,     /* optional put preamble */
        picelem[1025];          /* picture element following put */
    subraster *picelemsp = NULL,        /* rasterize picture elements */
        *picturesp = NULL,      /* subraster for entire picture */
        *oldworkingbox = workingbox;    /* save working box on entry */
    raster *picturerp = NULL;   /* raster for entire picture */
    int pixsz = 1;              /* pixels are one bit each */
    double x = 0.0, y = 0.0,    /* x,y-coords for put,multiput */
        xinc = 0.0, yinc = 0.0; /* x,y-incrementss for multiput */
    int width = 0, height = 0,  /* #pixels width,height of picture */
        ewidth = 0, eheight = 0,        /* pic element width,height */
        ix = 0, xpos = 0, iy = 0, ypos = 0,     /* mimeTeX x,y pixel coords */
        num = 1, inum;          /* number reps, index of element */
    int iscenter = 0;           /* center or lowerleft put position */
    int *oldworkingparam = workingparam,        /* save working param on entry */
        origin = 0;             /* x,yinc ++=00 +-=01 -+=10 --=11 */
/* -------------------------------------------------------------------------
First obtain (width,height) arguments immediately following \picture command
-------------------------------------------------------------------------- */
/* --- parse for (width,height) arguments, and bump expression past it --- */
    *expression = texsubexpr(*expression, putexpr, 254, "(", ")", 0, 0);
    if (*putexpr == '\0')
        goto end_of_job;        /* couldn't get (width,height) */
/* --- now interpret width,height returned in putexpr --- */
    if ((putptr = strchr(putexpr, ',')) != NULL)        /* look for ',' in width,height */
        *putptr = '\0';         /* found it, so replace ',' by '\0' */
    width = height = eround(putexpr);   /*width pixels */
    if (putptr != NULL)         /* 2nd arg, if present, is height */
        height = eround(putptr + 1);    /*in pixels */
/* -------------------------------------------------------------------------
Then obtain entire picture {...} subexpression following (width,height)
-------------------------------------------------------------------------- */
/* --- parse for picture subexpression, and bump expression past it --- */
    *expression = texsubexpr(*expression, picexpr, 2047, "{", "}", 0, 0);
    if (*picexpr == '\0')
        goto end_of_job;        /* couldn't get {pic_elements} */
/* -------------------------------------------------------------------------
allocate subraster and raster for complete picture
-------------------------------------------------------------------------- */
/* --- sanity check on width,height args --- */
    if (width < 2 || width > 600 || height < 2 || height > 600)
        goto end_of_job;
/* --- allocate and initialize subraster for constructed picture --- */
    if ((picturesp = new_subraster(width, height, pixsz))       /*allocate new subraster */
        == NULL)
        goto end_of_job;        /* quit if failed */
    workingbox = picturesp;     /* set workingbox to our picture */
/* --- initialize picture subraster parameters --- */
    picturesp->type = IMAGERASTER;      /* image */
    picturesp->symdef = NULL;   /* not applicable for image */
    picturesp->baseline = height / 2 + 2;       /* is a little above center good? */
    picturesp->size = size;     /* size (probably unneeded) */
    picturerp = picturesp->image;       /* raster embedded in subraster */
    if (msgfp != NULL && msglevel >= 29)        /* debugging */
        fprintf(msgfp, "picture> width,height=%d,%d\n", width, height);
/* -------------------------------------------------------------------------
parse out each picture element, rasterize it, and place it in picture
-------------------------------------------------------------------------- */
    while (*picptr != '\0') {   /* until we run out of pic_elems */
        /* -----------------------------------------------------------------------
           first obtain leading \[multi]put(x,y[;xinc,yinc;num]) args for pic_elem
           ------------------------------------------------------------------------ */
        /* --- init default values in case not explicitly supplied in args --- */
        x = y = 0.0;
        xinc = yinc = 0.0;
        num = 1;                /* init default values */
        iscenter = origin = 0;  /* center, origin */
        /* --- get (pream$x,y;xinc,yinc;num ) args and bump picptr past it --- */
        while (*picptr != '\0') /* skip invalid chars preceding ( */
            if (*picptr == '(')
                break;          /* found opening ( */
            else
                picptr++;       /* else skip invalid char */
        picptr = texsubexpr(picptr, putexpr, 254, "(", ")", 0, 0);
        if (*putexpr == '\0')
            goto end_of_job;    /* couldn't get (x,y) */
        /* --- first look for $-terminated or for any non-digit preamble --- */
        *pream = '\0';          /* init preamble as empty string */
        if ((putptr = strchr(putexpr, '$')) != NULL) {  /*check for $ pream terminator */
            *putptr++ = '\0';   /* replace $ by '\0', bump past $ */
            strninit(pream, putexpr, 92);
        } /* copy leading preamble from put */
        else {                  /* look for any non-digit preamble */
            int npream = 0;     /* #chars in preamble */
            for (preptr = pream, putptr = putexpr;; npream++, putptr++)
                if (*putptr == '\0'     /* end-of-putdata signalled */
                    || !isalpha((int) (*putptr))        /* or found non-alpha char */
                    ||npream > 92)
                    break;      /* or preamble too long */
                else
                    *preptr++ = *putptr;        /* copy alpha char to preamble */
            *preptr = '\0';
        }                       /* null-terminate preamble */
        /* --- interpret preamble --- */
        for (preptr = pream;; preptr++) /* examine each preamble char */
            if (*preptr == '\0')
                break;          /* end-of-preamble signalled */
            else
                switch (tolower(*preptr)) {     /* check lowercase preamble char */
                default:
                    break;      /* unrecognized flag */
                case 'c':
                    iscenter = 1;
                    break;      /* center pic_elem at x,y coords */
                }               /* --- end-of-switch --- */
        /* --- interpret x,y;xinc,yinc;num following preamble --- */
        if (*putptr != '\0') {  /*check for put data after preamble */
            /* --- first squeeze preamble out of put expression --- */
            if (*pream != '\0') {       /* have preamble */
                strsqueezep(putexpr, putptr);
            }
            /* squeeze it out */
            /* --- interpret x,y --- */
            if ((multptr = strchr(putexpr, ';')) != NULL)       /*semicolon signals multiput */
                *multptr = '\0';        /* replace semicolon by '\0' */
            if ((putptr = strchr(putexpr, ',')) != NULL)        /* comma separates x,y */
                *putptr = '\0'; /* replace comma by '\0'  */
            if (*putexpr != '\0')       /* leading , may be placeholder */
                x = (double) (eround(putexpr)); /* x coord in pixels */
            if (putptr != NULL) /* 2nd arg, if present, is y coord */
                y = (double) (eround(putptr + 1));      /* in pixels */
            /* --- interpret xinc,yinc,num if we have a multiput --- */
            if (multptr != NULL) {      /* found ';' signalling multiput */
                if ((preptr = strchr(multptr + 1, ';')) != NULL)        /* ';' preceding num arg */
                    *preptr = '\0';     /* replace ';' by '\0' */
                if ((putptr = strchr(multptr + 1, ',')) != NULL)        /* ',' between xinc,yinc */
                    *putptr = '\0';     /* replace ',' by '\0' */
                if (*(multptr + 1) != '\0')     /* leading , may be placeholder */
                    xinc = (double) (eround(multptr + 1));      /* xinc in pixels */
                if (putptr != NULL)     /* 2nd arg, if present, is yinc */
                    yinc = (double) (eround(putptr + 1));       /* in user pixels */
                num = (preptr == NULL ? 999 : atoi(preptr + 1));        /*explicit num val or 999 */
            }                   /* --- end-of-if(multptr!=NULL) --- */
        }                       /* --- end-of-if(*preptr!='\0') --- */
        if (msgfp != NULL && msglevel >= 29)    /* debugging */
            fprintf(msgfp,
                    "picture> pream;x,y;xinc,yinc;num=\"%s\";%.2f,%.2f;%.2f,%.2f;%d\n",
                    pream, x, y, xinc, yinc, num);
        /* -----------------------------------------------------------------------
           now obtain {...} picture element following [multi]put, and rasterize it
           ------------------------------------------------------------------------ */
        /* --- parse for {...} picture element and bump picptr past it --- */
        picptr = texsubexpr(picptr, picelem, 1023, "{", "}", 0, 0);
        if (*picelem == '\0')
            goto end_of_job;    /* couldn't get {pic_elem} */
        if (msgfp != NULL && msglevel >= 29)    /* debugging */
            fprintf(msgfp, "picture> picelem=\"%.50s\"\n", picelem);
        /* --- rasterize picture element --- */
        origin = 0;             /* init origin as working param */
        workingparam = &origin; /* and point working param to it */
        picelemsp = rasterize(picelem, size);   /* rasterize picture element */
        if (picelemsp == NULL)
            continue;           /* failed to rasterize, skip elem */
        ewidth = (picelemsp->image)->width;     /* width of element, in pixels */
        eheight = (picelemsp->image)->height;   /* height of element, in pixels */
        if (origin == 55)
            iscenter = 1;       /* origin set to (.5,.5) for center */
        if (msgfp != NULL && msglevel >= 29) {  /* debugging */
            fprintf(msgfp,
                    "picture> ewidth,eheight,origin,num=%d,%d,%d,%d\n",
                    ewidth, eheight, origin, num);
            if (msglevel >= 999)
                type_raster(picelemsp->image, msgfp);
        }
        /* -----------------------------------------------------------------------
           embed element in picture (once, or multiple times if requested)
           ------------------------------------------------------------------------ */
        for (inum = 0; inum < num; inum++) {    /* once, or for num repetitions */
            /* --- set x,y-coords for this iteration --- */
            ix = iround(x);
            iy = iround(y);     /* round x,y to nearest integer */
            if (iscenter) {     /* place center of element at x,y */
                xpos = ix - ewidth / 2; /* x picture coord to center elem */
                ypos = height - iy - eheight / 2;
            } /* y pixel coord to center elem */
            else {              /* default places lower-left at x,y */
                xpos = ix;      /* set x pixel coord for left */
                if (origin == 10 || origin == 11)       /* x,yinc's are -+ or -- */
                    xpos = ix - ewidth; /* so set for right instead */
                ypos = height - iy - eheight;   /* set y pixel coord for lower */
                if (origin == 1 || origin == 11)        /* x,yinc's are +- or -- */
                    ypos = height - iy;
            }                   /* so set for upper instead */
            if (msgfp != NULL && msglevel >= 29)        /* debugging */
                fprintf(msgfp,
                        "picture> inum,x,y,ix,iy,xpos,ypos=%d,%.2f,%.2f,%d,%d,%d,%d\n",
                        inum, x, y, ix, iy, xpos, ypos);
            /* --- embed token raster at xpos,ypos, and quit if out-of-bounds --- */
            if (!rastput(picturerp, picelemsp->image, ypos, xpos, 0))
                break;
            /* --- apply increment --- */
            if (xinc == 0. && yinc == 0.)
                break;          /* quit if both increments zero */
            x += xinc;
            y += yinc;          /* increment coords for next iter */
        }                       /* --- end-of-for(inum) --- */
        /* --- free picture element subraster after embedding it in picture --- */
        delete_subraster(picelemsp);    /* done with subraster, so free it */
    }                           /* --- end-of-while(*picptr!=0) --- */
/* -------------------------------------------------------------------------
return picture constructed from pic_elements to caller
-------------------------------------------------------------------------- */
  end_of_job:
    workingbox = oldworkingbox; /* restore original working box */
    workingparam = oldworkingparam;     /* restore original working param */
    return (picturesp);         /* return our picture to caller */
}                               /* --- end-of-function rastpicture() --- */


/* ==========================================================================
 * Function:    rastline ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \line handler, returns subraster corresponding to line
 *      parameters (xinc,yinc){xlen}
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \line to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \line
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to line
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \line(xinc,yinc){xlen}
 *        o if {xlen} not given, then it's assumed xlen = |xinc|
 * ======================================================================= */
/* --- entry point --- */
static subraster *rastline(const char **expression, int size,
                           subraster * basesp, int arg1, int arg2,
                           int arg3) {
    char linexpr[257], *xptr = linexpr; /*line(xinc,yinc){xlen} */
    subraster *linesp = NULL;   /* subraster for line */
    /*char  *origexpression = *expression; *//*original expression after \line */
    int pixsz = 1;              /* pixels are one bit each */
    int thickness = 1;          /* line thickness */
    double xinc = 0.0, yinc = 0.0,      /* x,y-increments for line, */
        xlen = 0.0, ylen = 0.0; /* x,y lengths for line */
    int width = 0, height = 0,  /* #pixels width,height of line */
        rwidth = 0, rheight = 0;        /*alloc width,height plus thickness */
    int istop = 0, isright = 0, /* origin at bot-left if x,yinc>=0 */
        origin = 0;             /* x,yinc: ++=00 +-=01 -+=10 --=11 */
/* -------------------------------------------------------------------------
obtain (xinc,yinc) arguments immediately following \line command
-------------------------------------------------------------------------- */
/* --- parse for (xinc,yinc) arguments, and bump expression past it --- */
    *expression = texsubexpr(*expression, linexpr, 253, "(", ")", 0, 0);
    if (*linexpr == '\0')
        goto end_of_job;        /* couldn't get (xinc,yinc) */
/* --- now interpret xinc,yinc;thickness returned in linexpr --- */
    if ((xptr = strchr(linexpr, ';')) != NULL) {        /* look for ';' after xinc,yinc */
        *xptr = '\0';           /* terminate linexpr at ; */
        thickness = evalterm(mimestore, xptr + 1);
    } /* get int thickness */ if ((xptr = strchr(linexpr, ',')) != NULL)        /* look for ',' in xinc,yinc */
        *xptr = '\0';           /* found it, so replace ',' by '\0' */
    if (*linexpr != '\0')       /* check against missing 1st arg */
        xinc = xlen = (double) evalterm(mimestore, linexpr);    /* xinc in user units */
    if (xptr != NULL)           /* 2nd arg, if present, is yinc */
        yinc = ylen = (double) evalterm(mimestore, xptr + 1);   /* in user units */
/* -------------------------------------------------------------------------
obtain optional {xlen} following (xinc,yinc), and calculate ylen
-------------------------------------------------------------------------- */
/* --- check if {xlen} given --- */
    if (*(*expression) == '{') {        /*have {xlen} if leading char is { */
        /* --- parse {xlen} and bump expression past it, interpret as double --- */
        *expression =
            texsubexpr(*expression, linexpr, 253, "{", "}", 0, 0);
        if (*linexpr == '\0')
            goto end_of_job;    /* couldn't get {xlen} */
        xlen = (double) evalterm(mimestore, linexpr);   /* xlen in user units */
        /* --- set other values accordingly --- */
        if (xlen < 0.0)
            xinc = -xinc;       /* if xlen negative, flip xinc sign */
        if (xinc != 0.0)
            ylen = xlen * yinc / xinc;  /* set ylen from xlen and slope */
        else
            xlen = 0.0;         /* can't have xlen if xinc=0 */
    }

    /* --- end-of-if(*(*expression)=='{') --- */
    /* -------------------------------------------------------------------------
       calculate width,height, etc, based on xlen,ylen, etc
       -------------------------------------------------------------------------- */
    /* --- force lengths positive --- */
    xlen = absval(xlen);        /* force xlen positive */
    ylen = absval(ylen);        /* force ylen positive */
/* --- calculate corresponding lengths in pixels --- */
    width = max2(1, iround(unitlength * xlen)); /*scale by unitlength and round, */
    height = max2(1, iround(unitlength * ylen));        /* and must be at least 1 pixel */
    rwidth = width + (ylen < 0.001 ? 0 : max2(0, thickness - 1));
    rheight = height + (xlen < 0.001 ? 0 : max2(0, thickness - 1));
/* --- set origin corner, x,yinc's: ++=0=(0,0) +-=1=(0,1) -+=10=(1,0) --- */
    if (xinc < 0.0)
        isright = 1;            /*negative xinc, so corner is (1,?) */
    if (yinc < 0.0)
        istop = 1;              /*negative yinc, so corner is (?,1) */
    origin = isright * 10 + istop;      /* interpret 0=(0,0), 11=(1,1), etc */
    if (msgfp != NULL && msglevel >= 29)        /* debugging */
        fprintf(msgfp,
                "rastline> width,height,origin;x,yinc=%d,%d,%d;%g,%g\n",
                width, height, origin, xinc, yinc);
/* -------------------------------------------------------------------------
allocate subraster and raster for line
-------------------------------------------------------------------------- */
/* --- sanity check on width,height,thickness args --- */
    if (width < 1 || width > 600
        || height < 1 || height > 600 || thickness < 1 || thickness > 25)
        goto end_of_job;
/* --- allocate and initialize subraster for constructed line --- */
    if ((linesp = new_subraster(rwidth, rheight, pixsz))        /* alloc new subraster */
        == NULL)
        goto end_of_job;        /* quit if failed */
/* --- initialize line subraster parameters --- */
    linesp->type = IMAGERASTER; /* image */
    linesp->symdef = NULL;      /* not applicable for image */
    linesp->baseline = height / 2 + 2   /* is a little above center good? */
        + (rheight - height) / 2;       /* account for line thickness too */
    linesp->size = size;        /* size (probably unneeded) */
/* -------------------------------------------------------------------------
draw the line
-------------------------------------------------------------------------- */
    line_raster(linesp->image,  /* embedded raster image */
                (istop ? 0 : height - 1),       /* row0, from bottom or top */
                (isright ? width - 1 : 0),      /* col0, from left or right */
                (istop ? height - 1 : 0),       /* row1, to top or bottom */
                (isright ? 0 : width - 1),      /* col1, to right or left */
                thickness);     /* line thickness (usually 1 pixel) */
/* -------------------------------------------------------------------------
return constructed line to caller
-------------------------------------------------------------------------- */
  end_of_job:
    if (workingparam != NULL)   /* caller wants origin */
        *workingparam = origin; /* return origin corner to caller */
    return (linesp);            /* return line to caller */
}                               /* --- end-of-function rastline() --- */


/* ==========================================================================
 * Function:    rastrule ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \rule handler, returns subraster corresponding to rule
 *      parameters [lift]{width}{height}
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \rule to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \rule
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to rule
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \rule[lift]{width}{height}
 *        o if [lift] not given, then bottom of rule on baseline
 *        o if width=0 then you get an invisible strut 1 (one) pixel wide
 * ======================================================================= */
static subraster *rastrule(const char **expression, int size,
                           subraster * basesp, int arg1, int arg2,
                           int arg3) {
    char rulexpr[257];          /* rule[lift]{wdth}{hgt} */
    subraster *rulesp = NULL;   /* subraster for rule */
    int pixsz = 1;              /* pixels are one bit each */
    int lift = 0, width = 0, height = 0;        /* default rule parameters */
    double dval;                /* convert ascii params to doubles */
    int rwidth = 0, rheight = 0;        /* alloc width, height plus lift */
/* -------------------------------------------------------------------------
Obtain lift,width,height
-------------------------------------------------------------------------- */
/* --- check for optional lift arg  --- */
    if (*(*expression) == '[') {        /*check for []-enclosed optional arg */
        *expression =
            texsubexpr(*expression, rulexpr, 255, "[", "]", 0, 0);
        dval = evalterm(mimestore, rulexpr);    /* convert [lift] to int */
        if (dval <= 99 && dval >= (-99))        /* sanity check */
            lift = iround(unitlength * dval);
    }
    /* scale by unitlength and round *//* --- parse for width --- */
        *expression =
        texsubexpr(*expression, rulexpr, 255, "{", "}", 0, 0);
    if (*rulexpr == '\0')
        goto end_of_job;        /* quit if args missing */
    dval = evalterm(mimestore, rulexpr);        /* convert {width} to int */
    if (dval <= 500 && dval >= 0)       /* sanity check */
        width = max2(0, iround(unitlength * dval));     /* scale by unitlength and round */
/* --- parse for height --- */
    *expression = texsubexpr(*expression, rulexpr, 255, "{", "}", 0, 0);
    if (*rulexpr == '\0')
        goto end_of_job;        /* quit if args missing */
    dval = evalterm(mimestore, rulexpr);        /* convert {height} to int */
    if (dval <= 500 && dval > 0)        /* sanity check */
        height = max2(1, iround(unitlength * dval));    /* scale by unitlength and round */
/* --- raster width,height in pixels --- */
    rwidth = max2(1, width);    /* raster must be at least 1 pixel */
    rheight = height + (lift >= 0 ? lift :      /* raster height plus lift */
                        (-lift < height ? 0 : -lift - height + 1));     /* may need empty space above rule */
/* -------------------------------------------------------------------------
allocate subraster and raster for rule
-------------------------------------------------------------------------- */
/* --- sanity check on width,height,thickness args --- */
    if (rwidth < 1 || rwidth > 600 || rheight < 1 || rheight > 600)
        goto end_of_job;
/* --- allocate and initialize subraster for constructed rule --- */
    if ((rulesp = new_subraster(rwidth, rheight, pixsz))        /* alloc new subraster */
        == NULL)
        goto end_of_job;        /* quit if failed */
/* --- initialize line subraster parameters --- */
    rulesp->type = IMAGERASTER; /* image */
    rulesp->symdef = NULL;      /* not applicable for image */
    rulesp->baseline = rheight - 1 + (lift >= 0 ? 0 : lift);    /*adjust baseline for lift */
    rulesp->size = size;        /* size (probably unneeded) */
/* -------------------------------------------------------------------------
draw the rule
-------------------------------------------------------------------------- */
    rule_raster(rulesp->image,  /* embedded raster image */
                (-lift < height ? 0 : rheight - height),        /* topmost row for top-left corner */
                0,              /* leftmost col for top-left corner */
                width,          /* rule width */
                height,         /* rule height */
                (width > 0 ? 0 : 4));   /* rule type */
/* -------------------------------------------------------------------------
return constructed rule to caller
-------------------------------------------------------------------------- */
  end_of_job:
    return (rulesp);            /* return rule to caller */
}                               /* --- end-of-function rastrule() --- */


/* ==========================================================================
 * Function:    rastcircle ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \circle handler, returns subraster corresponding to ellipse
 *      parameters (xdiam[,ydiam])
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \circle to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \circle
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to ellipse
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \circle(xdiam[,ydiam])
 *        o
 * ======================================================================= */
static subraster *rastcircle(const char **expression, int size,
                             subraster * basesp, int arg1, int arg2,
                             int arg3) {
    char circexpr[512], *xptr = circexpr;       /*circle(xdiam[,ydiam]) */
    char *qptr = NULL, quads[256] = "1234";     /* default to draw all quadrants */
    double theta0 = 0.0, theta1 = 0.0;  /* ;theta0,theta1 instead of ;quads */
    subraster *circsp = NULL;   /* subraster for ellipse */
    int pixsz = 1;              /* pixels are one bit each */
    double xdiam = 0.0, ydiam = 0.0;    /* x,y major/minor axes/diameters */
    int width = 0, height = 0;  /* #pixels width,height of ellipse */
    int thickness = 1;          /* drawn lines are one pixel thick */
    int origin = 55;            /* force origin centered */
/* -------------------------------------------------------------------------
obtain (xdiam[,ydiam]) arguments immediately following \circle command
-------------------------------------------------------------------------- */
/* --- parse for (xdiam[,ydiam]) args, and bump expression past it --- */
    *expression = texsubexpr(*expression, circexpr, 500, "(", ")", 0, 0);
    if (*circexpr == '\0')
        goto end_of_job;        /* couldn't get (xdiam[,ydiam]) */
/* --- now interpret xdiam[,ydiam] returned in circexpr --- */
    if ((qptr = strchr(circexpr, ';')) != NULL) {       /* semicolon signals quads data */
        *qptr = '\0';           /* replace semicolon by '\0' */
        strninit(quads, qptr + 1, 128); /* save user-requested quads */
        if ((qptr = strchr(quads, ',')) != NULL) {      /* have theta0,theta1 instead */
            *qptr = '\0';       /* replace , with null */
            theta0 = (double) evalterm(mimestore, quads);       /* theta0 precedes , */
            theta1 = (double) evalterm(mimestore, qptr + 1);    /* theta1 follows , */
            qptr = NULL;
        } /* signal thetas instead of quads */
        else
             qptr = quads;
    } /* set qptr arg for circle_raster() */
    else                        /* no ;quads at all */
        qptr = quads;           /* default to all 4 quadrants */
    if ((xptr = strchr(circexpr, ',')) != NULL) /* look for ',' in xdiam[,ydiam] */
        *xptr = '\0';           /* found it, so replace ',' by '\0' */
    xdiam = ydiam =             /* xdiam=ydiam in user units */
        (double) evalterm(mimestore, circexpr); /* evaluate expression */
    if (xptr != NULL)           /* 2nd arg, if present, is ydiam */
        ydiam = (double) evalterm(mimestore, xptr + 1); /* in user units */
/* -------------------------------------------------------------------------
calculate width,height, etc
-------------------------------------------------------------------------- */
/* --- calculate width,height in pixels --- */
    width = max2(1, iround(unitlength * xdiam));        /*scale by unitlength and round, */
    height = max2(1, iround(unitlength * ydiam));       /* and must be at least 1 pixel */
    if (msgfp != NULL && msglevel >= 29)        /* debugging */
        fprintf(msgfp, "rastcircle> width,height;quads=%d,%d,%s\n",
                width, height, (qptr == NULL ? "default" : qptr));
/* -------------------------------------------------------------------------
allocate subraster and raster for complete picture
-------------------------------------------------------------------------- */
/* --- sanity check on width,height args --- */
    if (width < 1 || width > 600 || height < 1 || height > 600)
        goto end_of_job;
/* --- allocate and initialize subraster for constructed ellipse --- */
    if ((circsp = new_subraster(width, height, pixsz))  /* allocate new subraster */
        == NULL)
        goto end_of_job;        /* quit if failed */
/* --- initialize ellipse subraster parameters --- */
    circsp->type = IMAGERASTER; /* image */
    circsp->symdef = NULL;      /* not applicable for image */
    circsp->baseline = height / 2 + 2;  /* is a little above center good? */
    circsp->size = size;        /* size (probably unneeded) */
/* -------------------------------------------------------------------------
draw the ellipse
-------------------------------------------------------------------------- */
    if (qptr != NULL)           /* have quads */
        circle_raster(circsp->image,    /* embedded raster image */
                      0, 0,     /* row0,col0 are upper-left corner */
                      height - 1, width - 1,    /* row1,col1 are lower-right */
                      thickness,        /* line thickness is 1 pixel */
                      qptr);    /* "1234" quadrants to be drawn */
    else                        /* have theta0,theta1 */
        circle_recurse(circsp->image,   /* embedded raster image */
                       0, 0,    /* row0,col0 are upper-left corner */
                       height - 1, width - 1,   /* row1,col1 are lower-right */
                       thickness,       /* line thickness is 1 pixel */
                       theta0, theta1); /* theta0,theta1 arc to be drawn */
/* -------------------------------------------------------------------------
return constructed ellipse to caller
-------------------------------------------------------------------------- */
  end_of_job:
    if (workingparam != NULL)   /* caller wants origin */
        *workingparam = origin; /* return center origin to caller */
    return (circsp);            /* return ellipse to caller */
}                               /* --- end-of-function rastcircle() --- */


/* ==========================================================================
 * Function:    rastbezier ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \bezier handler, returns subraster corresponding to bezier
 *      parameters (col0,row0)(col1,row1)(colt,rowt)
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \bezier to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \bezier
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to bezier
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \bezier(col1,row1)(colt,rowt)
 *        o col0=0,row0=0 assumed, i.e., given by
 *      \picture(){~(col0,row0){\bezier(col1,row1)(colt,rowt)}~}
 * ======================================================================= */
static subraster *rastbezier(const char **expression, int size,
                             subraster * basesp, int arg1, int arg2,
                             int arg3) {
    subraster *bezsp = NULL;    /* subraster for bezier */
    char bezexpr[129], *xptr = bezexpr; /*\bezier(r,c)(r,c)(r,c) */
    double r0 = 0.0, c0 = 0.0, r1 = 0.0, c1 = 0.0, rt = 0.0, ct = 0.0,  /* bezier points */
        rmid = 0.0, cmid = 0.0, /* coords at parameterized midpoint */
        rmin = 0.0, cmin = 0.0, /* minimum r,c */
        rmax = 0.0, cmax = 0.0, /* maximum r,c */
        rdelta = 0.0, cdelta = 0.0,     /* rmax-rmin, cmax-cmin */
        r = 0.0, c = 0.0;       /* some point */
    int iarg = 0;               /* 0=r0,c0 1=r1,c1 2=rt,ct */
    int width = 0, height = 0;  /* dimensions of bezier raster */
    int pixsz = 1;              /* pixels are one bit each */
    /*int   thickness = 1; *//* drawn lines are one pixel thick */
    int origin = 0;             /*c's,r's reset to lower-left origin */
/* -------------------------------------------------------------------------
obtain (c1,r1)(ct,rt) args immediately following \bezier command
-------------------------------------------------------------------------- */
    for (iarg = 1; iarg <= 2; iarg++) { /* 0=c0,r0 1=c1,r1 2=ct,rt */
        /* --- parse for (r,c) args, and bump expression past them all --- */
        *expression =
            texsubexpr(*expression, bezexpr, 127, "(", ")", 0, 0);
        if (*bezexpr == '\0')
            goto end_of_job;    /* couldn't get (r,c) */
        /* --- now interpret (r,c) returned in bezexpr --- */
        c = r = 0.0;            /* init x-coord=col, y-coord=row */
        if ((xptr = strchr(bezexpr, ',')) != NULL) {    /* comma separates row,col */
            *xptr = '\0';       /* found it, so replace ',' by '\0' */
            /* --- row=y-coord in pixels --- */
            r = unitlength * ((double) evalterm(mimestore, xptr + 1));
        }
        /* --- col=x-coord in pixels --- */
            c = unitlength * ((double) evalterm(mimestore, bezexpr));
        /* --- store r,c --- */
        switch (iarg) {
        case 0:
            r0 = r;
            c0 = c;
            break;
        case 1:
            r1 = r;
            c1 = c;
            break;
        case 2:
            rt = r;
            ct = c;
            break;
        }
    }                           /* --- end-of-for(iarg) --- */
/* --- determine midpoint and maximum,minimum points --- */
    rmid = 0.5 * (rt + 0.5 * (r0 + r1));        /* y-coord at middle of bezier */
    cmid = 0.5 * (ct + 0.5 * (c0 + c1));        /* x-coord at middle of bezier */
    rmin = min3(r0, r1, rmid);  /* lowest row */
    cmin = min3(c0, c1, cmid);  /* leftmost col */
    rmax = max3(r0, r1, rmid);  /* highest row */
    cmax = max3(c0, c1, cmid);  /* rightmost col */
    rdelta = rmax - rmin;       /* height */
    cdelta = cmax - cmin;       /* width */
/* --- rescale coords so we start at 0,0 --- */
    r0 -= rmin;
    c0 -= cmin;                 /* rescale r0,c0 */
    r1 -= rmin;
    c1 -= cmin;                 /* rescale r1,c1 */
    rt -= rmin;
    ct -= cmin;                 /* rescale rt,ct */
/* --- flip rows so 0,0 becomes lower-left corner instead of upper-left--- */
    r0 = rdelta - r0 + 1;       /* map 0-->height-1, height-1-->0 */
    r1 = rdelta - r1 + 1;
    rt = rdelta - rt + 1;
/* --- determine width,height of raster needed for bezier --- */
    width = (int) (cdelta + 0.9999) + 1;        /* round width up */
    height = (int) (rdelta + 0.9999) + 1;       /* round height up */
    if (msgfp != NULL && msglevel >= 29)        /* debugging */
        fprintf(msgfp,
                "rastbezier> width,height,origin=%d,%d,%d; c0,r0=%g,%g; "
                "c1,r1=%g,%g\n rmin,mid,max=%g,%g,%g; cmin,mid,max=%g,%g,%g\n",
                width, height, origin, c0, r0, c1, r1, rmin, rmid, rmax,
                cmin, cmid, cmax);
/* -------------------------------------------------------------------------
allocate raster
-------------------------------------------------------------------------- */
/* --- sanity check on width,height args --- */
    if (width < 1 || width > 600 || height < 1 || height > 600)
        goto end_of_job;
/* --- allocate and initialize subraster for constructed bezier --- */
    if ((bezsp = new_subraster(width, height, pixsz))   /* allocate new subraster */
        == NULL)
        goto end_of_job;        /* quit if failed */
/* --- initialize bezier subraster parameters --- */
    bezsp->type = IMAGERASTER;  /* image */
    bezsp->symdef = NULL;       /* not applicable for image */
    bezsp->baseline = height / 2 + 2;   /* is a little above center good? */
    bezsp->size = size;         /* size (probably unneeded) */
/* -------------------------------------------------------------------------
draw the bezier
-------------------------------------------------------------------------- */
    bezier_raster(bezsp->image, /* embedded raster image */
                  r0, c0,       /* row0,col0 are lower-left corner */
                  r1, c1,       /* row1,col1 are upper-right */
                  rt, ct);      /* bezier tangent point */
/* -------------------------------------------------------------------------
return constructed bezier to caller
-------------------------------------------------------------------------- */
  end_of_job:
    if (workingparam != NULL)   /* caller wants origin */
        *workingparam = origin; /* return center origin to caller */
    return bezsp;               /* return bezier to caller */
}                               /* --- end-of-function rastbezier() --- */


/* ==========================================================================
 * Function:    rastraise ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \raisebox{lift}{subexpression} handler, returns subraster
 *      containing subexpression with its baseline "lifted" by lift
 *      pixels, scaled by \unitlength, or "lowered" if lift arg
 *      negative
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \raisebox to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \raisebox
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to \raisebox
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \raisebox{lift}{subexpression}
 *        o
 * ======================================================================= */
static subraster *rastraise(const char **expression, int size,
                            subraster * basesp, int arg1, int arg2,
                            int arg3) {
    char subexpr[MAXSUBXSZ + 1], *liftexpr = subexpr;   /* args */
    subraster *raisesp = NULL;  /* rasterize subexpr to be raised */
    int lift = 0;               /* amount to raise/lower baseline */
/* -------------------------------------------------------------------------
obtain {lift} argument immediately following \raisebox command
-------------------------------------------------------------------------- */
     rastlift = 0;              /* reset global lift adjustment */
/* --- parse for {lift} arg, and bump expression past it --- */
    *expression = texsubexpr(*expression, liftexpr, 0, "{", "}", 0, 0);
    if (*liftexpr == '\0')
        goto end_of_job;        /* couldn't get {lift} */
     lift = eround(liftexpr);   /* {lift} to integer */
    if (abs(lift) > 200)
         lift = 0;              /* sanity check */
/* -------------------------------------------------------------------------
obtain {subexpr} argument after {lift}, and rasterize it
-------------------------------------------------------------------------- */
/* --- parse for {subexpr} arg, and bump expression past it --- */
    *expression = texsubexpr(*expression, subexpr, 0, "{", "}", 0, 0);
/* --- rasterize subexpression to be raised/lowered --- */
    if ((raisesp = rasterize(subexpr, size))    /* rasterize subexpression */
        == NULL)
        goto end_of_job;        /* and quit if failed */
/* -------------------------------------------------------------------------
raise/lower baseline and return it to caller
-------------------------------------------------------------------------- */
/* --- raise/lower baseline --- */
     raisesp->baseline += lift; /* new baseline (no height checks) */
     rastlift = lift;           /* set global to signal adjustment */
/* --- return raised subexpr to caller --- */
     end_of_job:return raisesp; /* return raised subexpr to caller */
}                               /* --- end-of-function rastraise() --- */
/* ==========================================================================
 * Function:    rastrotate ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \rotatebox{degrees}{subexpression} handler, returns subraster
 *      containing subexpression rotated by degrees (counterclockwise
 *      if degrees positive)
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \rotatebox to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \rotatebox
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to \rotatebox
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \rotatebox{degrees}{subexpression}
 *        o
 * ======================================================================= */
    subraster *rastrotate(const char **expression, int size,
                          subraster * basesp, int arg1, int arg2,
                          int arg3) {
    char subexpr[MAXSUBXSZ + 1], *degexpr = subexpr;    /* args */
    subraster *rotsp = NULL;    /* subraster for rotated subexpr */
    raster *rotrp = NULL;       /* rotate subraster->image 90 degs */
    int baseline = 0;           /* baseline of rasterized image */
    double degrees = 0.0, ipart, fpart; /* degrees to be rotated */
    int idegrees = 0, isneg = 0;        /* positive ipart, isneg=1 if neg */
    int n90 = 0, isn90 = 1;     /* degrees is n90 multiples of 90 */
/* -------------------------------------------------------------------------
obtain {degrees} argument immediately following \rotatebox command
-------------------------------------------------------------------------- */
/* --- parse for {degrees} arg, and bump expression past it --- */
    *expression = texsubexpr(*expression, degexpr, 0, "{", "}", 0, 0);
    if (*degexpr == '\0')
        goto end_of_job;        /* couldn't get {degrees} */
    degrees = (double) evalterm(mimestore, degexpr);    /* degrees to be rotated */
    if (degrees < 0.0) {        /* clockwise rotation desired */
        degrees = -degrees;     /* flip sign so degrees positive */
        isneg = 1;
    }                           /* and set flag to indicate flip */
    fpart = modf(degrees, &ipart);      /* integer and fractional parts */
    ipart = (double) (((int) degrees) % 360);   /* degrees mod 360 */
    degrees = ipart + fpart;    /* restore fractional part */
    if (isneg)                  /* if clockwise rotation requested */
        degrees = 360.0 - degrees;      /* do equivalent counterclockwise */
    idegrees = (int) (degrees + 0.5);   /* integer degrees */
    n90 = idegrees / 90;        /* degrees is n90 multiples of 90 */
    isn90 = (90 * n90 == idegrees);     /*true if degrees is multiple of 90 */
    isn90 = 1;                  /* forced true for time being */
/* -------------------------------------------------------------------------
obtain {subexpr} argument after {degrees}, and rasterize it
-------------------------------------------------------------------------- */
/* --- parse for {subexpr} arg, and bump expression past it --- */
    *expression = texsubexpr(*expression, subexpr, 0, "{", "}", 0, 0);
/* --- rasterize subexpression to be rotated --- */
    if ((rotsp = rasterize(subexpr, size))      /* rasterize subexpression */
        == NULL)
        goto end_of_job;        /* and quit if failed */
/* --- return unmodified image if no rotation requested --- */
    if (abs(idegrees) < 2)
        goto end_of_job;        /* don't bother rotating image */
/* --- extract params for image to be rotated --- */
    rotrp = rotsp->image;       /* unrotated rasterized image */
    baseline = rotsp->baseline; /* and baseline of that image */
/* -------------------------------------------------------------------------
rotate by multiples of 90 degrees
-------------------------------------------------------------------------- */
    if (isn90)                  /* rotation by multiples of 90 */
        if (n90 > 0) {          /* do nothing for 0 degrees */
            n90 = 4 - n90;      /* rasrot() rotates clockwise */
            while (n90 > 0) {   /* still have remaining rotations */
                raster *nextrp = rastrot(rotrp);        /* rotate raster image */
                if (nextrp == NULL)
                    break;      /* something's terribly wrong */
                delete_raster(rotrp);   /* free previous raster image */
                rotrp = nextrp; /* and replace it with rotated one */
                n90--;
            }                   /* decrement remaining count */
        }
    /* --- end-of-if(isn90) --- */
    /* -------------------------------------------------------------------------
       requested rotation not multiple of 90 degrees
       -------------------------------------------------------------------------- */
    if (!isn90) { /* explicitly construct rotation */ ;
    }

    /* not yet implemented */
    /* -------------------------------------------------------------------------
       re-populate subraster envelope with rotated image
       -------------------------------------------------------------------------- */
    /* --- re-init various subraster parameters, embedding raster in it --- */
    if (rotrp != NULL) {        /* rotated raster constructed okay */
        rotsp->type = IMAGERASTER;      /* signal constructed image */
        rotsp->image = rotrp;   /* raster we just constructed */
        /* --- now try to guess pleasing baseline --- */
        if (idegrees > 2) {     /* leave unchanged if unrotated */
            if (strlen(subexpr) < 3     /* we rotated a short expression */
                || abs(idegrees - 180) < 3)     /* or just turned it upside-down */
                baseline = rotrp->height - 1;   /* so set with nothing descending */
            else                /* rotated a long expression */
                baseline = (65 * (rotrp->height - 1)) / 100;
        }                       /* roughly center long expr */
        rotsp->baseline = baseline;
    }
    /* set baseline as calculated above */
    /* --- return rotated subexpr to caller --- */
  end_of_job:
    return rotsp;
}

/* ==========================================================================
 * Function:    rastmagnify ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \magnify{magstep}{subexpression} handler, returns subraster
 *      containing magnified subexpression
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \reflectbox to
 *              be rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \reflectbox
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to \magnify
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \magnify{magstep}{subexpression}
 *        o
 * ======================================================================= */
static subraster *rastmagnify(const char **expression, int size,
                              subraster * basesp, int arg1, int arg2,
                              int arg3) {
    char subexpr[MAXSUBXSZ + 1], *magexpr = subexpr;    /* args */
    subraster *magsp = NULL;    /* subraster for magnified subexpr */
    raster *magrp = NULL;       /* magnify subraster->image */
    int a_magstep = 1;          /* default magnification */
    int baseline = 0;           /* baseline of rasterized image */
/* -------------------------------------------------------------------------
obtain {magstep} argument immediately following \magnify command
-------------------------------------------------------------------------- */
/* --- parse for {magstep} arg, and bump expression past it --- */
    *expression = texsubexpr(*expression, magexpr, 255, "{", "}", 0, 0);
     a_magstep = atoi(magexpr); /* convert {magstep} to int */
    if (a_magstep < 1 || a_magstep > 10)        /* check magstep input */
         a_magstep = 1;         /* back to default if illegal */
/* -------------------------------------------------------------------------
obtain {subexpr} argument after {magstep}, and rasterize it
-------------------------------------------------------------------------- */
/* --- parse for {subexpr} arg, and bump expression past it --- */
    *expression = texsubexpr(*expression, subexpr, 0, "{", "}", 0, 0);
/* --- rasterize subexpression to be reflected --- */
    if ((magsp = rasterize(subexpr, size))      /* rasterize subexpression */
        == NULL)
        goto end_of_job;        /* and quit if failed */
/* --- return unmodified image if no magnification requested --- */
    if (a_magstep <= 1)
         goto end_of_job;       /* don't bother magnifying image */
/* --- extract params for image to be magnified --- */
     magrp = magsp->image;      /* unmagnified rasterized image */
     baseline = magsp->baseline;        /* and baseline of that image */
/* -------------------------------------------------------------------------
magnify image and adjust its parameters
-------------------------------------------------------------------------- */
/* --- magnify image --- */
     magrp = rastmag(magsp->image, a_magstep);  /* magnify raster image */
    if (magrp == NULL)
        goto end_of_job;        /* failed to magnify image */
     delete_raster(magsp->image);       /* free original raster image */
     magsp->image = magrp;      /*and replace it with magnified one */
/* --- adjust parameters --- */
     baseline *= a_magstep;     /* scale baseline */
    if (baseline > 0)
         baseline += 1;         /* adjust for no descenders */
     magsp->baseline = baseline;        /*reset baseline of magnified image */
/* --- return magnified subexpr to caller --- */
     end_of_job:return magsp;   /*back to caller with magnified expr */
}                               /* --- end-of-function rastmagnify() --- */
/* ==========================================================================
 * Function:    rastreflect ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \reflectbox[axis]{subexpression} handler, returns subraster
 *      containing subexpression reflected horizontally (i.e., around
 *      vertical axis, |_ becomes _|) if [axis] not given or axis=1,
 *      or reflected vertically if axis=2 given.
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \reflectbox to
 *              be rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \reflectbox
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to \reflectbox
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \reflectbox[axis]{subexpression}
 *        o
 * ======================================================================= */
    static subraster *rastreflect(const char **expression, int size,
                                  subraster * basesp, int arg1, int arg2,
                                  int arg3) {
    char subexpr[MAXSUBXSZ + 1], *axisexpr = subexpr;   /* args */
    subraster *refsp = NULL;    /* subraster for reflected subexpr */
    raster *refrp = NULL;       /* reflect subraster->image */
    int axis = 1;               /* default horizontal reflection */
    int baseline = 0;           /* baseline of rasterized image */
/* -------------------------------------------------------------------------
obtain [axis] argument immediately following \reflectbox command, if given
-------------------------------------------------------------------------- */
/* --- check for optional [axis] arg  --- */
    if (*(*expression) == '[') {        /*check for []-enclosed optional arg */
        *expression =
            texsubexpr(*expression, axisexpr, 255, "[", "]", 0, 0);
        axis = atoi(axisexpr);  /* convert [axis] to int */
        if (axis < 1 || axis > 2)       /* check axis input */
            axis = 1;
    }

    /* back to default if illegal *//* -------------------------------------------------------------------------
       obtain {subexpr} argument after optional [axis], and rasterize it
       -------------------------------------------------------------------------- *//* --- parse for {subexpr} arg, and bump expression past it --- */
        *expression = texsubexpr(*expression, subexpr, 0, "{", "}", 0, 0);
/* --- rasterize subexpression to be reflected --- */
    if ((refsp = rasterize(subexpr, size))      /* rasterize subexpression */
        == NULL)
        goto end_of_job;        /* and quit if failed */
/* --- return unmodified image if no reflection requested --- */
    if (axis < 1 || axis > 2)
        goto end_of_job;        /* don't bother reflecting image */
/* --- extract params for image to be reflected --- */
    refrp = refsp->image;       /* unreflected rasterized image */
    baseline = refsp->baseline; /* and baseline of that image */
/* -------------------------------------------------------------------------
reflect image and adjust its parameters
-------------------------------------------------------------------------- */
/* --- reflect image --- */
    refrp = rastref(refsp->image, axis);        /* reflect raster image */
    if (refrp == NULL)
        goto end_of_job;        /* failed to reflect image */
    delete_raster(refsp->image);        /* free original raster image */
    refsp->image = refrp;       /*and replace it with reflected one */
/* --- adjust parameters --- */
    if (axis == 2)              /* for vertical reflection */
        baseline = refrp->height - 1;   /* set with nothing descending */
    refsp->baseline = baseline; /* reset baseline of reflected image */
/* --- return reflected subexpr to caller --- */
  end_of_job:
    return refsp;               /*back to caller with reflected expr */
}                               /* --- end-of-function rastreflect() --- */


/* ==========================================================================
 * Function:    rastfbox ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \fbox{subexpression} handler, returns subraster
 *      containing subexpression with frame box drawn around it
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \fbox to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \fbox
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) ptr to subraster corresponding to \fbox
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \fbox[width][height]{subexpression}
 *        o
 * ======================================================================= */
subraster *rastfbox(const char **expression, int size, subraster * basesp,
                    int arg1, int arg2, int arg3) {
    char subexpr[MAXSUBXSZ + 1], widtharg[512]; /* args */
    subraster *framesp = NULL;  /* rasterize subexpr to be framed */
    raster *bp = NULL;          /* framed image raster */
    int evalue = 0;             /* interpret [width][height] */
    int fwidth = 6, fthick = 1; /*extra frame width, line thickness */
    int fsides = 0;             /* frame sides: 1=left,2=top,4=right,8=bot */
    int width = -1, height = -1;        /* optional [width][height] args */
    int iscompose = 0;          /* set true if optional args given */
/* -------------------------------------------------------------------------
obtain optional [width][height] arguments immediately following \fbox
-------------------------------------------------------------------------- */
/* --- first check for optional \fbox[width] --- */
    if (*(*expression) == '[') {        /* check for []-enclosed width arg */
        *expression =
            texsubexpr(*expression, widtharg, 511, "[", "]", 0, 0);
        if (!isempty(widtharg)) {       /* got widtharg */
            char *comma = strchr(widtharg, ',');        /* look for [width,sides] */
            if (comma == (char *) NULL) /* no comma */
                comma = strchr(widtharg, ';');  /* permit semicolon [width;sides] */
            if (comma != (char *) NULL) {       /* optional [width,fsides] found */
                fsides = atoi(comma + 1);       /* interpret fsides after comma */
                if (size < 5) { /* for smaller fonts */
                    fwidth = 2;
                    fthick = 1;
                } /* tighten frame, thinner accent */
                else {
                    fwidth = 3;
                    fthick = 2;
                }               /* loosen frame, thicken accent */
                *comma = '\0';  /* null-terminate width at comma */
                trimwhite(widtharg);
            }                   /*remove leading/trailing whitespace */
            if (comma == (char *) NULL || !isempty(widtharg)) { /* have a width */
                height = 1;     /* default explicit height, too */
                if (fsides == 0) {      /* a normal framebox */
                    evalue = eround(widtharg);  /* interpret and scale width */
                    width = max2(1, evalue);    /* must be >0 */
                    fwidth = 2;
                    iscompose = 1;
                } else          /* absolute pixels for "accents" */
                    width = evalterm(mimestore, widtharg);
            }
        }                       /* --- end-of-if(!isempty(widtharg)) --- */
    }                           /* --- end-of-if(**expression=='[') --- */
    if (width > 0 || fsides > 0)        /* found leading [width], so... */
        if (*(*expression) == '[') {    /* check for []-enclosed height arg */
            *expression =
                texsubexpr(*expression, widtharg, 511, "[", "]", 0, 0);
            if (!isempty(widtharg)) {   /* got widtharg */
                if (fsides == 0) {      /* a normal framebox */
                    evalue = eround(widtharg);  /* interpret and scale height */
                    height = max2(1, evalue);   /* must be >0 */
                    fwidth = 0;
                } /* no extra border */
                else            /* absolute pixels for "accents" */
                    height = evalterm(mimestore, widtharg);
            }
        }

    /* --- end-of-if(**expression=='[') --- */
    /* -------------------------------------------------------------------------
       obtain {subexpr} argument
       -------------------------------------------------------------------------- */
    /* --- parse for {subexpr} arg, and bump expression past it --- */
    *expression = texsubexpr(*expression, subexpr, 0, "{", "}", 0, 0);
/* --- rasterize subexpression to be framed --- */
    if (width < 0 || height < 0) {      /* no explicit dimensions given */
        if ((framesp = rasterize(subexpr, size))        /* rasterize subexpression */
            == NULL)
            goto end_of_job;
    } /* and quit if failed */
    else {
        char composexpr[8192];  /* compose subexpr with empty box */
        sprintf(composexpr, "\\compose{\\hspace{%d}\\vspace{%d}}{%.8000s}",
                width, height, subexpr);
        if ((framesp = rasterize(composexpr, size))     /* rasterize subexpression */
            == NULL)
            goto end_of_job;
    }                           /* and quit if failed */
/* -------------------------------------------------------------------------
draw frame, reset params, and return it to caller
-------------------------------------------------------------------------- */
/* --- draw border --- */
    if (fsides > 0)
        fthick += (100 * fsides);       /* embed fsides in fthick arg */
    if ((bp = border_raster(framesp->image, -fwidth, -fwidth, fthick, 1))
        == NULL)
        goto end_of_job;        /* draw border and quit if failed */
/* --- replace original image and raise baseline to accommodate frame --- */
    framesp->image = bp;        /* replace image with framed one */
    if (!iscompose)             /* simple border around subexpr */
        framesp->baseline += fwidth;    /* so just raise baseline */
    else
        framesp->baseline = (framesp->image)->height - 1;       /* set at bottom */
/* --- return framed subexpr to caller --- */
  end_of_job:
    return framesp;
}

/* ==========================================================================
 * Function:    rasteval ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: handle \eval
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \eval,
 *              and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \eval
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) subraster ptr to date stamp
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster *rasteval(const char **expression, int size,
                           subraster * basesp, int arg1, int arg2,
                           int arg3) {
    char subexpr[MAXSUBXSZ];    /* arg to be evaluated */
    subraster *evalsp = NULL;   /* rasterize evaluated expression */
    int value = 0;              /* evaluate expression */
/* -------------------------------------------------------------------------
Parse for subexpr to be \eval-uated, and bump expression past it
-------------------------------------------------------------------------- */
    *expression = texsubexpr(*expression, subexpr, 0, "{", "}", 0, 0);
    if (*subexpr == '\0')       /* couldn't get subexpression */
        goto end_of_job;        /* nothing to do, so quit */
/* -------------------------------------------------------------------------
Evaluate expression, ascii-ize integer result, rasterize it
-------------------------------------------------------------------------- */
/* --- evaluate expression --- */
     value = evalterm(mimestore, subexpr);      /* evaluate expression */
/* --- ascii-ize it --- */
     sprintf(subexpr, "%d", value);     /* ascii version of value */
/* --- rasterize ascii-ized expression value --- */
     evalsp = rasterize(subexpr, size); /* rasterize evaluated expression */
/* --- return evaluated expression raster to caller --- */
     end_of_job:return (evalsp);
}
/* ==========================================================================
 * Function:    rastnoop ( expression, size, basesp, nargs, arg2, arg3 )
 * Purpose: no op -- flush \escape without error
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \escape to be
 *              flushed, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-5 default font size
 *      basesp (I)  subraster *  to character (or subexpression)
 *              immediately preceding \escape
 *              (unused, but passed for consistency)
 *      nargs (I)   int containing number of {}-args after
 *              \escape to be flushed along with it
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster * ) NULL subraster ptr
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
    static subraster *rastnoop(const char **expression, int size,
                               subraster * basesp, int nargs, int arg2,
                               int arg3) {
    char subexpr[MAXSUBXSZ + 1];        /*dummy args eaten by \escape */
    subraster *noopsp = NULL;   /* rasterize subexpr */
/* --- flush accompanying args if necessary --- */
    if (nargs != NOVALUE && nargs > 0) {        /* and args to be flushed */
        while (--nargs >= 0) {  /* count down */
            *expression = texsubexpr(*expression, subexpr, 0, "{", "}", 0, 0);  /*flush arg */
    }}
                                                                                /* --- return null ptr to caller --- *//*end_of_job:*/ return noopsp;
                                                                                /* return NULL ptr to caller */
}

/* ==========================================================================
 * Function:    strwrap ( s, linelen, tablen )
 * Purpose: Inserts \n's and spaces in (a copy of) s to wrap lines
 *      at linelen and indent them by tablen.
 * --------------------------------------------------------------------------
 * Arguments:   s (I)       char * to null-terminated string
 *              to be wrapped.
 *      linelen (I) int containing maximum linelen
 *              between \\'s.
 *      tablen (I)  int containing number of spaces to indent
 *              lines.  0=no indent.  Positive means
 *              only indent first line and not others.
 *              Negative means indent all lines except first.
 * --------------------------------------------------------------------------
 * Returns: ( char * )  ptr to "line-wrapped" copy of s
 *              or "" (empty string) for any error.
 * --------------------------------------------------------------------------
 * Notes:     o The returned copy of s has embedded \\'s as necessary
 *      to wrap lines at linelen.  Any \\'s in the input copy
 *      are removed first.  If (and only if) the input s contains
 *      a terminating \\ then so does the returned copy.
 *        o The returned pointer addresses a static buffer,
 *      so don't call strwrap() again until you're finished
 *      with output from the preceding call.
 *        o Modified for mimetex from original version written
 *      for mathtex (where \n in verbatim mode instead of \\
 *      produced linebreaks).
 * ======================================================================= */
char *strwrap(const char *s, int linelen, int tablen) {
    static char sbuff[4096];    /* line-wrapped copy of s */
    char *sol = sbuff;          /* ptr to start of current line */
    char tab[32] = "                 "; /* tab string */
    int finalnewline = (lastchar(s) == '\n' ? 1 : 0);   /*newline at end of string? */
    int istab = (tablen > 0 ? 1 : 0),   /* init true to indent first line */
        iswhite = 0;            /* true if line break on whitespace */
    int rhslen = 0,             /* remaining right hand side length */
        thislen = 0,            /* length of current line segment */
        thistab = 0,            /* length of tab on current line */
        wordlen = 0;            /* length to next whitespace char */
/* -------------------------------------------------------------------------
Make a clean copy of s
-------------------------------------------------------------------------- */
/* --- check input --- */
    *sbuff = '\0';              /* initialize in case of error */
    if (isempty(s))
         goto end_of_job;       /* no input */
    if (tablen < 0)
         tablen = (-tablen);    /* set positive tablen */
    if (tablen >= linelen)
         tablen = linelen - 1;  /* tab was longer than line */
     tab[min2(tablen, 16)] = '\0';      /* null-terminate tab string */
     tablen = strlen(tab);      /* reset to actual tab length */
     finalnewline = 0;          /* turned off for mimetex version */
/* --- start with copy of s --- */
     strninit(sbuff, s, 3000);  /* leave room for \n's and tabs */
    if (linelen < 1)
         goto end_of_job;       /* can't do anything */
     trimwhite(sbuff);          /*remove leading/trailing whitespace */
     strreplace(sbuff, "\n", " ", 0);   /* remove any original \n's */
     strreplace(sbuff, "\r", " ", 0);   /* remove any original \r's */
     strreplace(sbuff, "\t", " ", 0);   /* remove any original \t's */
     strreplace(sbuff, "\f", " ", 0);   /* remove any original \f's */
     strreplace(sbuff, "\v", " ", 0);   /* remove any original \v's */
     strreplace(sbuff, "\\\\", " ", 0); /* remove any original \\'s */
/* -------------------------------------------------------------------------
Insert \\'s and spaces as needed
-------------------------------------------------------------------------- */
    while (1) {                 /* till end-of-line */
        /* --- init --- */
        trimwhite(sol);         /*remove leading/trailing whitespace */
        thislen = thistab = 0;  /* no chars in current line yet */
        if (istab && tablen > 0) {      /* need to indent this line */
            strchange(0, sol, tab);     /* insert indent at start of line */
            thistab = tablen;
        } /* line starts with whitespace tab */ if (sol == sbuff)
            istab = 1 - istab;  /* flip tab flag after first line */
        sol += thistab;         /* skip tab */
        rhslen = strlen(sol);   /* remaining right hand side chars */
        if (rhslen + thistab <= linelen)
            break;              /* no more \\'s needed */
        if (0 && msgfp != NULL && msglevel >= 99) {
            fprintf(msgfp, "strwrap> rhslen=%d, sol=\"\"%s\"\"\n", rhslen,
                    sol);
            fflush(msgfp);
        }
        /* --- look for last whitespace preceding linelen --- */
        while (1) {             /* till we exceed linelen */
            wordlen = strcspn(sol + thislen, " \t\n\r\f\v :;.,");       /*ptr to next white/break */
            if (sol[thislen + wordlen] == '\0') /* no more whitespace in string */
                goto end_of_job;        /* so nothing more we can do */
            if (thislen + thistab + wordlen >= linelen) /* next word won't fit */
                if (thislen > 0)
                    break;      /* but make sure line has one word */
            thislen += (wordlen + 1);
        }                       /* ptr past next whitespace char */
        if (thislen < 1)
            break;              /* line will have one too-long word */
        /*sol[thislen-1] = '\n'; *//* replace last space with newline */
        /*sol += thislen; *//* next line starts after newline */
        iswhite = (isthischar(sol[thislen - 1], ":;.,") ? 0 : 1);       /*linebreak on space? */
        strchange(iswhite, sol + thislen - iswhite, "\\\\");    /* put \\ at end of line */
        sol += (thislen + 2 - iswhite); /* next line starts after \\ */
    }                           /* --- end-of-while(1) --- */
  end_of_job:
    if (finalnewline)
        strcat(sbuff, "\\\\");  /* replace final newline */
    return (sbuff);             /* back with clean copy of s */
}                               /* --- end-of-function strwrap() --- */

/* ==========================================================================
 * Function:    aalowpass ( rp, bytemap, grayscale )
 * Purpose: calculates a lowpass anti-aliased bytemap
 *      for rp->bitmap, with each byte 0...grayscale-1
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster whose bitmap
 *              is to be anti-aliased
 *      bytemap (O) intbyte * to bytemap, calculated
 *              by applying lowpass filter to rp->bitmap,
 *              and returned (as you'd expect) in 1-to-1
 *              addressing correspondence with rp->bitmap
 *      grayscale (I)   int containing number of grayscales
 *              to be calculated, 0...grayscale-1
 *              (should typically be given as 256)
 * --------------------------------------------------------------------------
 * Returns: ( int )     1=success, 0=any error
 * --------------------------------------------------------------------------
 * Notes:     o If the center point of the box being averaged is black,
 *      then the entire "average" is forced black (grayscale-1)
 *      regardless of the surrounding points.  This is my attempt
 *      to avoid a "washed-out" appearance of thin (one-pixel-wide)
 *      lines, which would otherwise turn from black to a gray shade.
 *       o  Also, while the weights for neighbor points are fixed,
 *      you may adjust the center point weight on the compile line.
 *      A higher weight sharpens the resulting anti-aliased image;
 *      lower weights blur it out more (but keep the "center" black
 *      as per the preceding note).
 * ======================================================================= */
int aalowpass(raster * rp, intbyte * bytemap, int grayscale) {
    int status = 1;             /* 1=success, 0=failure to caller */
    pixbyte *bitmap = (rp == NULL ? NULL : rp->pixmap); /*local rp->pixmap ptr */
    int irow = 0, icol = 0;     /* rp->height, rp->width indexes */
    int weights[9] = {
    1, 3, 1, 3, 0, 3, 1, 3, 1}; /* matrix of weights */
    int adjindex[9] = {
    0, 1, 2, 7, -1, 3, 6, 5, 4};        /*clockwise from upper-left */
    int totwts = 0;             /* sum of all weights in matrix */
    int isforceavg = 1,         /*force avg black if center black? */
        isminmaxwts = 1,        /*use wts or #pts for min/max test */
        blackscale = 0;         /*(grayscale+1)/4; *//*force black if wgted avg>bs */
/* -------------------------------------------------------------------------
Initialization
-------------------------------------------------------------------------- */
/* --- calculate total weights --- */
    weights[4] = centerwt;      /* weight for center point */
    weights[1] = weights[3] = weights[5] = weights[7] = adjacentwt;     /*adjacent pts */
    totwts = centerwt + 4 * (1 + adjacentwt);   /* tot is center plus neighbors */
/* -------------------------------------------------------------------------
Calculate bytemap as 9-point weighted average over bitmap
-------------------------------------------------------------------------- */
    for (irow = 0; irow < rp->height; irow++) {
        for (icol = 0; icol < rp->width; icol++) {
            int ipixel = icol + irow * (rp->width);     /* center pixel index */
            int jrow = 0, jcol = 0,     /* box around ipixel */
                bitval = 0,     /* value of bit/pixel at jrow,jcol */
                iscenter = 0,   /* set true if center pixel black */
                nadjacent = 0, wadjacent = 0,   /* #adjacent black pixels, their wts */
                ngaps = 0,      /* #gaps in 8 pixels around center */
                iwt = (-1), sumwts = 0; /* weights index, sum in-bound wts */
            char adjmatrix[8];  /* adjacency "matrix" */
            memset(adjmatrix, 0, 8);    /* zero out adjacency matrix */
            bytemap[ipixel] = 0;        /* init pixel white */
  /*--- for ipixel at irow,icol, get weighted average of adjacent pixels ---*/
            for (jrow = irow - 1; jrow <= irow + 1; jrow++)     /* jrow = irow-1...irow+1 */
                for (jcol = icol - 1; jcol <= icol + 1; jcol++) {       /* jcol = icol-1...icol+1 */
                    int jpixel = jcol + jrow * (rp->width);     /* averaging index */
                    iwt++;      /*always bump weight index */
                    if (jrow < 0 || jrow >= rp->height  /* if row out pf bounds */
                        || jcol < 0 || jcol >= rp->width)       /* or col out of bounds */
                        continue;       /* ignore this point */
                    bitval = (int) getlongbit(bitmap, jpixel);  /* value of bit at jrow,jcol */
                    if (bitval) {       /* this is a black pixel */
                        if (jrow == irow && jcol == icol)       /* and this is center point */
                            iscenter = 1;       /* set flag for center point black */
                        else {  /* adjacent point black */
                            nadjacent++;        /* bump adjacent black count */
                            adjmatrix[adjindex[iwt]] = 1;
                        }       /*set "bit" in adjacency matrix */
                        wadjacent += weights[iwt];
                    }           /* sum weights for black pixels */
                    sumwts += weights[iwt];     /* and sum weights for all pixels */
                }               /* --- end-of-for(jrow,jcol) --- */
            /* --- count gaps --- */
            ngaps = (adjmatrix[7] != adjmatrix[0] ? 1 : 0);     /* init count */
            for (iwt = 0; iwt < 7; iwt++)       /* clockwise around adjacency */
                if (adjmatrix[iwt] != adjmatrix[iwt + 1])
                    ngaps++;    /* black/white flip */
            ngaps /= 2;         /*each gap has 2 black/white flips */
            /* --- anti-alias pixel, but leave it black if it was already black --- */
            if (isforceavg && iscenter) /* force avg if center point black */
                bytemap[ipixel] = grayscale - 1;        /* so force grayscale-1=black */
            else /* center point not black */ if (ngaps <= 2) { /*don't darken checkerboarded pixel */
                bytemap[ipixel] =       /* 0=white ... grayscale-1=black */
                    ((totwts / 2 - 1) + (grayscale - 1) * wadjacent) / totwts;  /* not /sumwts; */
                if (blackscale > 0      /* blackscale kludge turned on */
                    && bytemap[ipixel] > blackscale)    /* weighted avg > blackscale */
                    bytemap[ipixel] = grayscale - 1;
            }
            /* so force it entirely black */
 /*--- only anti-alias pixels whose adjacent pixels fall within bounds ---*/
            if (!iscenter) {    /* apply min/maxadjacent test */
                if (isminmaxwts) {      /* min/max refer to adjacent weights */
                    if (wadjacent < minadjacent /* wts of adjacent points too low */
                        || wadjacent > maxadjacent)     /* or too high */
                        bytemap[ipixel] = 0;
                } /* so leave point white */
                else {          /* min/max refer to #adjacent points */
                    if (nadjacent < minadjacent /* too few adjacent points black */
                        || nadjacent > maxadjacent)     /* or too many */
                        bytemap[ipixel] = 0;
                }
            }                   /* so leave point white */
        }
    }
/*end_of_job:*/
    return status;
}

/* ==========================================================================
 * Function:    aapnm ( rp, bytemap, grayscale )
 * Purpose: calculates a lowpass anti-aliased bytemap
 *      for rp->bitmap, with each byte 0...grayscale-1,
 *      based on the pnmalias.c algorithm
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster whose bitmap
 *              is to be anti-aliased
 *      bytemap (O) intbyte * to bytemap, calculated
 *              by applying pnm-based filter to rp->bitmap,
 *              and returned (as you'd expect) in 1-to-1
 *              addressing correspondence with rp->bitmap
 *      grayscale (I)   int containing number of grayscales
 *              to be calculated, 0...grayscale-1
 *              (should typically be given as 256)
 * --------------------------------------------------------------------------
 * Returns: ( int )     1=success, 0=any error
 * --------------------------------------------------------------------------
 * Notes:    o  Based on the pnmalias.c algorithm in the netpbm package
 *      on sourceforge.
 * ======================================================================= */
int aapnm(raster * rp, intbyte * bytemap, int grayscale) {
    pixbyte *bitmap = rp->pixmap;       /* local rp->pixmap ptr */
    int width = rp->width, height = rp->height, /* width, height of raster */
        icol = 0, irow = 0,     /* width, height indexes */
        imap = (-1);            /* pixel index = icol + irow*width */
    int bgbitval = 0, fgbitval = 1;     /* background, foreground bitval */
    int isfirstaa = 1;          /*debugging switch signals 1st pixel */
#if 0
    int totwts = 12, wts[9] = {
    1, 1, 1, 1, 4, 1, 1, 1, 1}; /* pnmalias default wts */
    int totwts = 16, wts[9] = {
    1, 2, 1, 2, 4, 2, 1, 2, 1}; /* weights */
#endif
    int totwts = 18, wts[9] = {
    1, 2, 1, 2, 6, 2, 1, 2, 1}; /* pnmalias default wts */
    int isresetparams = 1,      /* true to set antialiasing params */
        isfgalias = 1,          /* true to antialias fg bits */
        isfgonly = 0,           /* true to only antialias fg bits */
        isbgalias = 0,          /* true to antialias bg bits */
        isbgonly = 0;           /* true to only antialias bg bits */
/* -------------------------------------------------------------------------
Initialization
-------------------------------------------------------------------------- */
/* --- check for bold light --- */
    if (0) {
        if (weightnum > 2) {
            isbgalias = 1;
        }                       /* simulate bold */
        if (weightnum < 1) {
            isbgonly = 1;
            isfgalias = 0;
        }
    }
    /* simulate light */
    /* --- reset wts[], etc, and calculate total weights --- */
    if (isresetparams) {        /* wts[], etc taken from params */
        int iwt = 0;            /* wts[iwt] index */
        wts[4] = centerwt;      /* weight for center point */
        wts[1] = wts[3] = wts[5] = wts[7] = adjacentwt; /* and adjacent points */
        wts[0] = wts[2] = wts[6] = wts[8] = cornerwt;   /* and corner points */
        for (totwts = 0, iwt = 0; iwt < 9; iwt++)
            totwts += wts[iwt]; /* sum wts */
        isfgalias = fgalias;    /* set isfgalias */
        isfgonly = fgonly;      /* set isfgonly */
        isbgalias = bgalias;    /* set isbgalias */
        isbgonly = bgonly;
    }
    /* set isbgonly */
    /* -------------------------------------------------------------------------
       Calculate bytemap as 9-point weighted average over bitmap
       -------------------------------------------------------------------------- */
    for (irow = 0; irow < height; irow++)
        for (icol = 0; icol < width; icol++) {
            /* --- local allocations and declarations --- */
            int bitval = 0,     /* value of rp bit at irow,icol */
                nnbitval = 0, nebitval = 0, eebitval = 0, sebitval = 0, /*adjacent vals */
                ssbitval = 0, swbitval = 0, wwbitval = 0, nwbitval = 0; /*compass pt names */
            int isbgedge = 0, isfgedge = 0;     /*does pixel border a bg or fg edge */
            int aabyteval = 0;  /* antialiased (or unchanged) value */
            /* --- bump imap index and get center bit value --- */
            imap++;             /* imap = icol + irow*width */
            bitval = getlongbit(bitmap, imap);  /* value of rp input bit at imap */
            aabyteval = (intbyte) (bitval == bgbitval ? 0 : grayscale - 1);     /* default aa val */
            bytemap[imap] = (intbyte) (aabyteval);      /* init antialiased pixel */
            /* --- check if we're antialiasing this pixel --- */
            if ((isbgonly && bitval == fgbitval)        /* only antialias background bit */
                ||(isfgonly && bitval == bgbitval))     /* only antialias foreground bit */
                continue;       /* leave default and do next bit */
            /* --- get surrounding bits --- */
            if (irow > 0)       /* nn (north) bit available */
                nnbitval = getlongbit(bitmap, imap - width);    /* nn bit value */
            if (irow < height - 1)      /* ss (south) bit available */
                ssbitval = getlongbit(bitmap, imap + width);    /* ss bit value */
            if (icol > 0) {     /* ww (west) bit available */
                wwbitval = getlongbit(bitmap, imap - 1);        /* ww bit value */
                if (irow > 0)   /* nw bit available */
                    nwbitval = getlongbit(bitmap, imap - width - 1);    /* nw bit value */
                if (irow < height - 1)  /* sw bit available */
                    swbitval = getlongbit(bitmap, imap + width - 1);
            }                   /* sw bit value */
            if (icol < width - 1) {     /* ee (east) bit available */
                eebitval = getlongbit(bitmap, imap + 1);        /* ee bit value */
                if (irow > 0)   /* ne bit available */
                    nebitval = getlongbit(bitmap, imap - width + 1);    /* ne bit value */
                if (irow < height - 1)  /* se bit available */
                    sebitval = getlongbit(bitmap, imap + width + 1);
            }
            /* se bit value */
            /* --- check for edges --- */
            isbgedge =          /* current pixel borders a bg edge */
                (nnbitval == bgbitval && eebitval == bgbitval) ||       /*upper-right edge */
                (eebitval == bgbitval && ssbitval == bgbitval) ||       /*lower-right edge */
                (ssbitval == bgbitval && wwbitval == bgbitval) ||       /*lower-left  edge */
                (wwbitval == bgbitval && nnbitval == bgbitval); /*upper-left  edge */
            isfgedge =          /* current pixel borders an fg edge */
                (nnbitval == fgbitval && eebitval == fgbitval) ||       /*upper-right edge */
                (eebitval == fgbitval && ssbitval == fgbitval) ||       /*lower-right edge */
                (ssbitval == fgbitval && wwbitval == fgbitval) ||       /*lower-left  edge */
                (wwbitval == fgbitval && nnbitval == fgbitval); /*upper-left  edge */
            /* ---check top/bot left/right edges for corners (added by j.forkosh)--- */
            if (1) {            /* true to perform test */
                int isbghorz = 0, isfghorz = 0, isbgvert = 0, isfgvert = 0;     /* horz/vert edges */
                isbghorz =      /* top or bottom edge is all bg */
                    (nwbitval + nnbitval + nebitval == 3 * bgbitval) || /* top edge bg */
                    (swbitval + ssbitval + sebitval == 3 * bgbitval);   /* bottom edge bg */
                isfghorz =      /* top or bottom edge is all fg */
                    (nwbitval + nnbitval + nebitval == 3 * fgbitval) || /* top edge fg */
                    (swbitval + ssbitval + sebitval == 3 * fgbitval);   /* bottom edge fg */
                isbgvert =      /* left or right edge is all bg */
                    (nwbitval + wwbitval + swbitval == 3 * bgbitval) || /* left edge bg */
                    (nebitval + eebitval + sebitval == 3 * bgbitval);   /* right edge bg */
                isfgvert =      /* left or right edge is all bg */
                    (nwbitval + wwbitval + swbitval == 3 * fgbitval) || /* left edge fg */
                    (nebitval + eebitval + sebitval == 3 * fgbitval);   /* right edge fg */
                if ((isbghorz && isbgvert && (bitval == fgbitval))      /* we're at an... */
                    ||(isfghorz && isfgvert && (bitval == bgbitval)))   /*...inside corner */
                    continue;   /* don't antialias */
            }
            /* --- end-of-if(1) --- */
            /* --- check #gaps for checkerboard (added by j.forkosh) --- */
            if (0) {            /* true to perform test */
                int ngaps = 0, mingaps = 1, maxgaps = 2;        /* count #fg/bg flips (max=4 noop) */
                if (nwbitval != nnbitval)
                    ngaps++;    /* upper-left =? upper */
                if (nnbitval != nebitval)
                    ngaps++;    /* upper =? upper-right */
                if (nebitval != eebitval)
                    ngaps++;    /* upper-right =? right */
                if (eebitval != sebitval)
                    ngaps++;    /* right =? lower-right */
                if (sebitval != ssbitval)
                    ngaps++;    /* lower-right =? lower */
                if (ssbitval != swbitval)
                    ngaps++;    /* lower =? lower-left */
                if (swbitval != wwbitval)
                    ngaps++;    /* lower-left =? left */
                if (wwbitval != nwbitval)
                    ngaps++;    /* left =? upper-left */
                if (ngaps > 0)
                    ngaps /= 2; /* each gap has 2 bg/fg flips */
                if (ngaps < mingaps || ngaps > maxgaps)
                    continue;
            }
            /* --- end-of-if(1) --- */
            /* --- antialias if necessary --- */
            if ((isbgalias && isbgedge) /* alias pixel surrounding bg */
                ||(isfgalias && isfgedge)       /* alias pixel surrounding fg */
                ||(isbgedge && isfgedge)) {     /* neighboring fg and bg pixel */
                int aasumval =  /* sum wts[]*bitmap[] */
                    wts[0] * nwbitval + wts[1] * nnbitval +
                    wts[2] * nebitval + wts[3] * wwbitval +
                    wts[4] * bitval + wts[5] * eebitval +
                    wts[6] * swbitval + wts[7] * ssbitval +
                    wts[8] * sebitval;
                double aawtval = ((double) aasumval) / ((double) totwts);       /* weighted val */
                aabyteval = (int) (((double) (grayscale - 1)) * aawtval + 0.5); /*0...grayscale-1 */
                bytemap[imap] = (intbyte) (aabyteval);  /* set antialiased pixel */
                if (msglevel >= 99 && msgfp != NULL) {
                    fprintf(msgfp,      /*diagnostic output */
                            "%s> irow,icol,imap=%d,%d,%d aawtval=%.4f aabyteval=%d\n",
                            (isfirstaa ? "aapnm algorithm" : "aapnm"),
                            irow, icol, imap, aawtval, aabyteval);
                    isfirstaa = 0;
                }
            }                   /* --- end-of-if(isedge) --- */
        }                       /* --- end-of-for(irow,icol) --- */
/* -------------------------------------------------------------------------
Back to caller with gray-scale anti-aliased bytemap
-------------------------------------------------------------------------- */
/*end_of_job:*/
    return 1;
}

/* ==========================================================================
 * Function:    aapnmlookup ( rp, bytemap, grayscale )
 * Purpose: calculates a lowpass anti-aliased bytemap
 *      for rp->bitmap, with each byte 0...grayscale-1,
 *      based on the pnmalias.c algorithm.
 *      This version uses aagridnum() and aapatternnum() lookups
 *      to interpret 3x3 lowpass pixel grids.
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster whose bitmap
 *              is to be anti-aliased
 *      bytemap (O) intbyte * to bytemap, calculated
 *              by applying pnm-based filter to rp->bitmap,
 *              and returned (as you'd expect) in 1-to-1
 *              addressing correspondence with rp->bitmap
 *      grayscale (I)   int containing number of grayscales
 *              to be calculated, 0...grayscale-1
 *              (should typically be given as 256)
 * --------------------------------------------------------------------------
 * Returns: ( int )     1=success, 0=any error
 * --------------------------------------------------------------------------
 * Notes:    o  Based on the pnmalias.c algorithm in the netpbm package
 *      on sourceforge.
 *       o  This version uses aagridnum() and aapatternnum() lookups
 *      to interpret 3x3 lowpass pixel grids.
 * ======================================================================= */
int aapnmlookup(raster * rp, intbyte * bytemap, int grayscale) {
    int width = rp->width, height = rp->height, /* width, height of raster */
        icol = 0, irow = 0,     /* width, height indexes */
        imap = (-1);            /* pixel index = icol + irow*width */
    int bgbitval = 0, fgbitval = 1;     /* background, foreground bitval */
    int isfirstaa = 1;          /*debugging switch signals 1st pixel */
    int aacenterwt = centerwt, aaadjacentwt = adjacentwt, aacornerwt = cornerwt, totwts = centerwt + 4 * (adjacentwt + cornerwt);       /*pnmalias default wts */
    int isfgalias = fgalias,    /*(1) true to antialias fg bits */
        isfgonly = fgonly,      /*(0) true to only antialias fg bits */
        isbgalias = bgalias,    /*(0) true to antialias bg bits */
        isbgonly = bgonly;      /*(0) true to only antialias bg bits */
    int gridnum = -1;           /* grid# for 3x3 grid at irow,icol */
    int patternum = -1;         /*pattern#, 1-51, for input gridnum */
/* ---
 * pattern number data
 * ------------------- */
/* --- number of adjacent fg pixels set in pattern --- */
    static int nadjacents[] = {
        -1,                     /* #adjacent fg pixels for pattern */
            0, 4, 0, 1, 4, 3, 1, 0, 1, 0,       /*  1-10 */
            2, 2, 3, 4, 3, 4, 2, 2, 1, 2,       /* 11-20 */
            1, 2, 1, 2, 0, 1, 3, 2, 3, 2,       /* 21-30 */
            3, 2, 3, 2, 4, 3, 1, 2, 2, 2,       /* 31-40 */
            2, 1, 2, 2, 3, 0, 3, 2, 2, 1, 4,    /* 41-51 */
    -1};                        /* --- end-of-nadjacents[] --- */
/* --- number of corner fg pixels set in pattern --- */
    static int ncorners[] = {
        -1,                     /* #corner fg pixels for pattern */
            0, 4, 1, 0, 3, 4, 1, 2, 1, 2,       /*  1-10 */
            0, 0, 3, 2, 3, 2, 4, 4, 2, 1,       /* 11-20 */
            2, 1, 2, 1, 3, 2, 0, 1, 2, 3,       /* 21-30 */
            2, 3, 2, 3, 1, 2, 4, 3, 2, 2,       /* 31-40 */
            2, 3, 2, 2, 1, 4, 1, 2, 2, 3, 0,    /* 41-51 */
    -1};                        /* --- end-of-ncorners[] --- */
/* --- 0,1,2=pattern contains horizontal bg,fg,both edge; -1=no edge --- */
    static int horzedges[] = {
        -1,                     /* 0,1,2 = horz bg,fg,both edge */
            0, 1, 0, 0, 1, 1, 0, 0, 0, -1,      /*  1-10 */
            0, -1, 1, 1, 1, -1, 1, -1, 2, 0,    /* 11-20 */
            -1, -1, -1, 0, -1, -1, -1, -1, 2, 1,        /* 21-30 */
            -1, -1, -1, 1, -1, -1, -1, -1, 2, -1,       /* 31-40 */
            -1, 1, 1, -1, -1, -1, 0, 0, -1, -1, -1,     /* 41-51 */
    -1};                        /* --- end-of-horzedges[] --- */
/* --- 0,1,2=pattern contains vertical bg,fg,both edge; -1=no edge --- */
    static int vertedges[] = {
        -1,                     /* 0,1,2 = vert bg,fg,both edge */
            0, 1, 0, 0, 1, 1, 0, -1, -1, -1,    /*  1-10 */
            0, 0, 1, -1, -1, -1, 1, 1, -1, -1,  /* 11-20 */
            -1, 0, 0, 0, -1, -1, 0, -1, -1, -1, /* 21-30 */
            -1, 1, 1, 1, -1, -1, 1, -1, -1, -1, /* 31-40 */
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 41-51 */
    -1};                        /* --- end-of-vertedges[] --- */
/* --- 0,1,2=pattern contains diagonal bg,fg,both edge; -1=no edge --- */
    static int diagedges[] = {
        -1,                     /* 0,1,2 = diag bg,fg,both edge */
            0, 1, 0, 0, 1, 1, 0, 0, 0, 0,       /*  1-10 */
            2, -1, 1, 1, 1, 1, 1, -1, 0, 2,     /* 11-20 */
            0, -1, 0, 2, 0, -1, 1, 1, 1, 1,     /* 21-30 */
            1, -1, 1, 2, 1, 1, -1, 1, 2, -1,    /* 31-40 */
            1, 0, -1, 2, 1, 0, 1, -1, 1, -1, 1, /* 41-51 */
    -1};                        /* --- end-of-diagedges[] --- */
/* -------------------------------------------------------------------------
Calculate bytemap as 9-point weighted average over bitmap
-------------------------------------------------------------------------- */
    for (irow = 0; irow < height; irow++)
        for (icol = 0; icol < width; icol++) {
            /* --- local allocations and declarations --- */
            int bitval = 0,     /* value of rp bit at irow,icol */
                isbgdiag = 0, isfgdiag = 0,     /*does pixel border a bg or fg edge */
                aabyteval = 0;  /* antialiased (or unchanged) value */
            /* --- get gridnum and center bit value, init aabyteval --- */
            imap++;             /* first set imap=icol + irow*width */
            gridnum = aagridnum(rp, irow, icol);        /*grid# coding 3x3 grid at irow,icol */
            bitval = (gridnum & 1);     /* center bit set if gridnum odd */
            aabyteval = (intbyte) (bitval == bgbitval ? 0 : grayscale - 1);     /* default aa val */
            bytemap[imap] = (intbyte) (aabyteval);      /* init antialiased pixel */
            if (gridnum < 0 || gridnum > 511)
                continue;       /* gridnum out of bounds */
            /* --- check if we're antialiasing this pixel --- */
            if ((isbgonly && bitval == fgbitval)        /* only antialias background bit */
                ||(isfgonly && bitval == bgbitval))     /* only antialias foreground bit */
                continue;       /* leave default and do next bit */
            /* --- look up pattern number, 1-51, corresponding to input gridnum --- */
            patternum = aapatternnum(gridnum);  /* look up pattern number */
            if (patternum < 1 || patternum > 51)
                continue;       /* some internal error */
            /* --- special pattern number processing --- */
            if ((aabyteval = aapatterns(rp, irow, icol, gridnum, patternum, grayscale)) >= 0) { /* special processing for pattern */
                bytemap[imap] = (intbyte) (aabyteval);  /* set antialiased pixel */
                continue;
            }
            /* and continue with next pixel */
            /* --- check for diagonal edges --- */
            isbgdiag = (diagedges[patternum] == 2 ||    /*current pixel borders a bg edge */
                        diagedges[patternum] == 0);
            isfgdiag = (diagedges[patternum] == 2 ||    /*current pixel borders a fg edge */
                        diagedges[patternum] == 1);
            /* ---check top/bot left/right edges for corners (added by j.forkosh)--- */
            if (1) {            /* true to perform test */
                int isbghorz = 0, isfghorz = 0, isbgvert = 0, isfgvert = 0,     /* horz/vert edges */
                    horzedge = horzedges[patternum], vertedge =
                    vertedges[patternum];
                isbghorz = (horzedge == 2 || horzedge == 0);    /* top or bottom edge is all bg */
                isfghorz = (horzedge == 2 || horzedge == 1);    /* top or bottom edge is all fg */
                isbgvert = (vertedge == 2 || vertedge == 0);    /* left or right edge is all bg */
                isfgvert = (vertedge == 2 || vertedge == 1);    /* left or right edge is all fg */
                if ((isbghorz && isbgvert && (bitval == fgbitval))      /* we're at an... */
                    ||(isfghorz && isfgvert && (bitval == bgbitval)))   /*...inside corner */
                    continue;   /* don't antialias */
            }                   /* --- end-of-if(1) --- */
#if 0
            /* --- check #gaps for checkerboard (added by j.forkosh) --- */
            if (0) {            /* true to perform test */
                int ngaps = 0, mingaps = 1, maxgaps = 2;        /* count #fg/bg flips (max=4 noop) */
                if (nwbitval != nnbitval)
                    ngaps++;    /* upper-left =? upper */
                if (nnbitval != nebitval)
                    ngaps++;    /* upper =? upper-right */
                if (nebitval != eebitval)
                    ngaps++;    /* upper-right =? right */
                if (eebitval != sebitval)
                    ngaps++;    /* right =? lower-right */
                if (sebitval != ssbitval)
                    ngaps++;    /* lower-right =? lower */
                if (ssbitval != swbitval)
                    ngaps++;    /* lower =? lower-left */
                if (swbitval != wwbitval)
                    ngaps++;    /* lower-left =? left */
                if (wwbitval != nwbitval)
                    ngaps++;    /* left =? upper-left */
                if (ngaps > 0)
                    ngaps /= 2; /* each gap has 2 bg/fg flips */
                if (ngaps < mingaps || ngaps > maxgaps)
                    continue;
            }                   /* --- end-of-if(1) --- */
#endif
            /* --- antialias if necessary --- */
            if ((isbgalias && isbgdiag) /* alias pixel surrounding bg */
                ||(isfgalias && isfgdiag)       /* alias pixel surrounding fg */
                ||(isbgdiag && isfgdiag)) {     /* neighboring fg and bg pixel */
                int aasumval =  /* sum wts[]*bitmap[] */
                    aacenterwt * bitval +       /* apply centerwt to center pixel */
                    aaadjacentwt * nadjacents[patternum] +      /* similarly for adjacents */
                    aacornerwt * ncorners[patternum];   /* and corners */
                double aawtval = ((double) aasumval) / ((double) totwts);       /* weighted val */
                aabyteval = (int) (((double) (grayscale - 1)) * aawtval + 0.5); /*0...grayscale-1 */
                bytemap[imap] = (intbyte) (aabyteval);  /* set antialiased pixel */
                if (msglevel >= 99 && msgfp != NULL) {
                    fprintf(msgfp,      /*diagnostic output */
                            "%s> irow,icol,imap=%d,%d,%d aawtval=%.4f aabyteval=%d",
                            (isfirstaa ? "aapnmlookup algorithm" :
                             "aapnm"), irow, icol, imap, aawtval,
                            aabyteval);
                    if (msglevel < 100)
                        fprintf(msgfp, "\n");   /* no more output */
                    else
                        fprintf(msgfp, ", grid#,pattern#=%d,%d\n", gridnum,
                                patternum);
                    isfirstaa = 0;
                }
            }                   /* --- end-of-if(isedge) --- */
        }                       /* --- end-of-for(irow,icol) --- */
/* -------------------------------------------------------------------------
Back to caller with gray-scale anti-aliased bytemap
-------------------------------------------------------------------------- */
/*end_of_job:*/
    return 1;
}

/* ==========================================================================
 * Function:    aapatterns ( rp, irow, icol, gridnum, patternum, grayscale )
 * Purpose: For patterns requireing special processing,
 *      calculates anti-aliased value for pixel at irow,icol,
 *      whose surrounding 3x3 pixel grid is coded by gridnum
 *      (which must correspond to a pattern requiring special
 *      processing).
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster whose bitmap
 *              is to be anti-aliased
 *      irow (I)    int containing row, 0...height-1,
 *              of pixel to be antialiased
 *      icol (I)    int containing col, 0...width-1,
 *              of pixel to be antialiased
 *      gridnum (I) int containing 0...511 corresponding to
 *              3x3 pixel grid surrounding irow,icol
 *      patternum (I)   int containing 1...51 pattern# of
 *              the 3x3 grid surrounding irow,icol
 *      grayscale (I)   int containing number of grayscales
 *              to be calculated, 0...grayscale-1
 *              (should typically be given as 256)
 * --------------------------------------------------------------------------
 * Returns: ( int )     0...grayscale-1 for success,
 *              -1 = error, or no special processing required
 * --------------------------------------------------------------------------
 * Notes:    o
 * ======================================================================= */
int aapatterns(raster * rp, int irow, int icol, int gridnum, int patternum,
               int grayscale) {
    int aaval = -1;             /* antialiased value returned */
    int iscenter = (gridnum & 1);       /* true if center pixel set/black */
/* -------------------------------------------------------------------------
special pattern number processing
-------------------------------------------------------------------------- */
    if (1) {
        if (patternum < 1)      /* pattern# not supplied by caller */
            patternum = aapatternnum(gridnum);  /* so look it up ourselves */
        switch (patternum) {
        default:
            break;              /* no special processing */
            case 11:case 24:aaval =
                aapattern1124(rp, irow, icol, gridnum, grayscale);
            break;
            case 19:aaval =
                aapattern19(rp, irow, icol, gridnum, grayscale);
            break;
            case 20:aaval =
                aapattern20(rp, irow, icol, gridnum, grayscale);
            break;
            case 39:aaval =
                aapattern39(rp, irow, icol, gridnum, grayscale);
            break;
            /* case 24: if ( (gridnum&1) == 0 ) aaval=0; break; */
            case 29:aaval = (iscenter ? grayscale - 1 : 0);
            break;              /* no antialiasing */
    }}
    return aaval;
}
#endif

/* ==========================================================================
 * Function:    aapattern1124 ( rp, irow, icol, gridnum, grayscale )
 * Purpose: calculates anti-aliased value for pixel at irow,icol,
 *      whose surrounding 3x3 pixel grid is coded by gridnum
 *      (which must correspond to pattern #11 or #24).
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster whose bitmap
 *              is to be anti-aliased
 *      irow (I)    int containing row, 0...height-1,
 *              of pixel to be antialiased
 *      icol (I)    int containing col, 0...width-1,
 *              of pixel to be antialiased
 *      gridnum (I) int containing 0...511 corresponding to
 *              3x3 pixel grid surrounding irow,icol
 *      grayscale (I)   int containing number of grayscales
 *              to be calculated, 0...grayscale-1
 *              (should typically be given as 256)
 * --------------------------------------------------------------------------
 * Returns: ( int )     0...grayscale-1 for success, -1=any error
 * --------------------------------------------------------------------------
 * Notes:    o  Handles the eight gridnum's
 *      (gridnum/2 shown to eliminate irrelevant low-order bit)
 *        ---        ---         -*-          -*-
 *        --* = 10   *-- = 18    --* = 72     *-- = 80  (pattern$11)
 *        -*-        -*-         ---          ---
 *
 *        ---        ---         -**          **-
 *        --* = 11   *-- = 22    --* = 104    *-- = 208 (pattern$24)
 *        -**        **-         ---          ---
 *       o  For black * center pixel, using grid#10 as an example,
 *      pixel stays ---      antialiased  ---*
 *      black if    -***     if part of   -**
 *      part of a   -*-      a diagonal   -*- 
 *      corner, eg,  *       line, eg,    *
 * ======================================================================= */
static int aapattern1124(raster * rp, int irow, int icol, int gridnum,
                         int grayscale) {
    int aaval = -1;             /* antialiased value returned */
    int iscenter = gridnum & 1; /* true if pixel at irow,icol black */
    int patternum = 24;         /* init for pattern#24 default */
    pixbyte *bitmap = rp->pixmap;       /* local rp->pixmap ptr */
    int width = rp->width, height = rp->height; /* width, height of raster */
    int jrow = irow, jcol = icol;       /* corner or diagonal row,col */
    int vertcornval = 0, horzcornval = 0;       /* vertical, horizontal corner bits */
    int topdiagval = 0, botdiagval = 0; /* upper,lower diagonal pixel bits */
    int /* not used --cornval = 0, */ diagval = 0;      /* vert+horzcorn, top+botdiag */
    int hdirection = 99, vdirection = 99;       /* horz,vert corner direction */
    int hturn = 99, vturn = 99, aafollowline(); /* follow corner till turns */
/* -------------------------------------------------------------------------
Check corner and diagonal pixels
-------------------------------------------------------------------------- */
    if (0) {
        goto end_of_job;        /* true to turn off pattern1124 */
    }
    switch (gridnum / 2) {      /* check pattern#11,24 corner, diag */
    default:
        goto end_of_job;        /* not a pattern#11,24 gridnum */
    case 10:
        patternum = 11;
    case 11:
        hdirection = 2;
        vdirection = -1;        /* directions to follow corner */
        if ((jrow = irow + 2) < height) {       /* vert corner below center pixel */
            vertcornval = getlongbit(bitmap, (icol + jrow * width));
            if ((icol - 1) >= 0)        /* lower diag left of center */
                botdiagval =
                    getlongbit(bitmap, ((icol - 1) + jrow * width));
        }
        if ((jcol = icol + 2) < width) {        /* horz corner right of center */
            horzcornval = getlongbit(bitmap, (jcol + irow * width));
            if ((irow - 1) >= 0)        /* upper diag above center */
                topdiagval =
                    getlongbit(bitmap, (jcol + (irow - 1) * width));
        }
        break;
    case 18:
        patternum = 11;
    case 22:
        hdirection = -2;
        vdirection = -1;        /* directions to follow corner */
        if ((jrow = irow + 2) < height) {       /* vert corner below center pixel */
            vertcornval = getlongbit(bitmap, (icol + jrow * width));
            if ((icol + 1) < width)     /* lower diag right of center */
                botdiagval =
                    getlongbit(bitmap, ((icol + 1) + jrow * width));
        }
        if ((jcol = icol - 2) >= 0) {   /* horz corner left of center */
            horzcornval = getlongbit(bitmap, (jcol + irow * width));
            if ((irow - 1) >= 0)        /* upper diag above center */
                topdiagval =
                    getlongbit(bitmap, (jcol + (irow - 1) * width));
        }
        break;
    case 72:
        patternum = 11;
    case 104:
        hdirection = 2;
        vdirection = 1;         /* directions to follow corner */
        if ((jrow = irow - 2) >= 0) {   /* vert corner above center pixel */
            vertcornval = getlongbit(bitmap, (icol + jrow * width));
            if ((icol - 1) >= 0)        /* upper diag left of center */
                topdiagval =
                    getlongbit(bitmap, ((icol - 1) + jrow * width));
        }
        if ((jcol = icol + 2) < width) {        /* horz corner right of center */
            horzcornval = getlongbit(bitmap, (jcol + irow * width));
            if ((irow + 1) < height)    /* lower diag below center */
                botdiagval =
                    getlongbit(bitmap, (jcol + (irow + 1) * width));
        }
        break;
    case 80:
        patternum = 11;
    case 208:
        hdirection = -2;
        vdirection = 1;         /* directions to follow corner */
        if ((jrow = irow - 2) >= 0) {   /* vert corner above center pixel */
            vertcornval = getlongbit(bitmap, (icol + jrow * width));
            if ((icol + 1) < width)     /* upper diag right of center */
                topdiagval =
                    getlongbit(bitmap, ((icol + 1) + jrow * width));
        }
        if ((jcol = icol - 2) >= 0) {   /* horz corner left of center */
            horzcornval = getlongbit(bitmap, (jcol + irow * width));
            if ((irow + 1) < height)    /* lower diag below center */
                botdiagval =
                    getlongbit(bitmap, (jcol + (irow + 1) * width));
        }
        break;
    }                           /* --- end-of-switch(gridnum/2) --- */
    /* not used --cornval = vertcornval + horzcornval; *//* 0=no corner bits, 1, 2=both */
    diagval = topdiagval + botdiagval;  /* 0=no diag bits, 1, 2=both */
/* -------------------------------------------------------------------------
Handle white center
-------------------------------------------------------------------------- */
    if (1 && !iscenter) {
        aaval = (patternum == 11 ? 51 : 64);
        goto end_of_job;
    }
/* -------------------------------------------------------------------------
Handle black center
-------------------------------------------------------------------------- */
    if (diagval > 1)
        aaval = (patternum == 24 ? 255 : 191);
    else {
        hturn = aafollowline(rp, irow, icol, hdirection);
        vturn = aafollowline(rp, irow, icol, vdirection);
        if (vturn * hdirection < 0 && hturn * vdirection < 0)
            aaval = (patternum == 24 ? 255 : 191);
        else
            aaval = grayscale - 1;
    }                           /* actual corner */
/* -------------------------------------------------------------------------
Back to caller with grayscale antialiased value for pixel at irow,icol
-------------------------------------------------------------------------- */
  end_of_job:
    if (aaval >= 0)             /* have antialiasing result */
        if (msglevel >= 99 && msgfp != NULL)
            fprintf(msgfp,      /* diagnostic output */
                    "aapattern1124> irow,icol,grid#/2=%d,%d,%d, top,botdiag=%d,%d, "
                    "vert,horzcorn=%d,%d, v,hdir=%d,%d, v,hturn=%d,%d, aaval=%d\n",
                    irow, icol, gridnum / 2, topdiagval, botdiagval,
                    vertcornval, horzcornval, vdirection, hdirection,
                    vturn, hturn, aaval);
    return (aaval);             /* back with antialiased value */
}                               /* --- end-of-function aapattern1124() --- */


/* ==========================================================================
 * Function:    aapattern19 ( rp, irow, icol, gridnum, grayscale )
 * Purpose: calculates anti-aliased value for pixel at irow,icol,
 *      whose surrounding 3x3 pixel grid is coded by gridnum
 *      (which must correspond to pattern #19).
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster whose bitmap
 *              is to be anti-aliased
 *      irow (I)    int containing row, 0...height-1,
 *              of pixel to be antialiased
 *      icol (I)    int containing col, 0...width-1,
 *              of pixel to be antialiased
 *      gridnum (I) int containing 0...511 corresponding to
 *              3x3 pixel grid surrounding irow,icol
 *      grayscale (I)   int containing number of grayscales
 *              to be calculated, 0...grayscale-1
 *              (should typically be given as 256)
 * --------------------------------------------------------------------------
 * Returns: ( int )     0...grayscale-1 for success, -1=any error
 * --------------------------------------------------------------------------
 * Notes:    o  Handles the four gridnum's
 *      (gridnum/2 shown to eliminate irrelevant low-order bit)
 *        ---        --*         *--          ***
 *        --- = 7    --* = 41    *-- = 148    --- = 224
 *        ***        --*         *--          ---
 * ======================================================================= */
static int aapattern19(raster * rp, int irow, int icol, int gridnum,
                       int grayscale) {
    int aaval = (-1);           /* antialiased value returned */
    int iscenter = gridnum & 1; /* true if pixel at irow,icol black */
    int orientation = 1,        /* 1=vertical, 2=horizontal */
        jrow = irow, jcol = icol;       /* middle pixel of *** line */
    int turn1 = 0, turn2 = 0, aafollowline();   /* follow *** line till it turns */
/* -------------------------------------------------------------------------
Initialization and calculation of antialiased value
-------------------------------------------------------------------------- */
/* --- check input -- */
    if (iscenter)
         goto end_of_job;       /* we only antialias white pixels */
/* --- set params --- */
    switch (gridnum / 2) {      /* check pattern#19 orientation */
    default:
        goto end_of_job;        /* not a pattern#19 gridnum */
        case 7:orientation = 2;
        jrow++;
        break;
        case 41:orientation = 1;
        jcol++;
        break;
        case 148:orientation = 1;
        jcol--;
        break;
        case 224:orientation = 2;
        jrow--;
        break;
    }                           /* --- end-of-switch(gridnum/2) --- */
/* --- get turns in both directions --- */
        if ((turn1 = aafollowline(rp, jrow, jcol, orientation)) == 0)
        goto end_of_job;
    if ((turn2 = aafollowline(rp, jrow, jcol, -orientation)) == 0)
        goto end_of_job;
    if (turn1 * turn2 >= 0)
        goto end_of_job;        /* both turns in same direction */
/* --- weight pixel --- */
    aaval = grayscale / (3 + min2(abs(turn1), abs(turn2)));
/* -------------------------------------------------------------------------
Back to caller with grayscale antialiased value for pixel at irow,icol
-------------------------------------------------------------------------- */
  end_of_job:
    if (aaval >= 0)             /* have antialiasing result */
        if (msglevel >= 99 && msgfp != NULL)
            fprintf(msgfp,      /* diagnostic output */
                    "aapattern19> irow,icol,grid#/2=%d,%d,%d, turn+%d,%d=%d,%d, aaval=%d\n",
                    irow, icol, gridnum / 2, orientation, -orientation,
                    turn1, turn2, aaval);
    return (aaval);             /* back with antialiased value */
}                               /* --- end-of-function aapattern19() --- */


/* ==========================================================================
 * Function:    aapattern20 ( rp, irow, icol, gridnum, grayscale )
 * Purpose: calculates anti-aliased value for pixel at irow,icol,
 *      whose surrounding 3x3 pixel grid is coded by gridnum
 *      (which must correspond to pattern #20).
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster whose bitmap
 *              is to be anti-aliased
 *      irow (I)    int containing row, 0...height-1,
 *              of pixel to be antialiased
 *      icol (I)    int containing col, 0...width-1,
 *              of pixel to be antialiased
 *      gridnum (I) int containing 0...511 corresponding to
 *              3x3 pixel grid surrounding irow,icol
 *      grayscale (I)   int containing number of grayscales
 *              to be calculated, 0...grayscale-1
 *              (should typically be given as 256)
 * --------------------------------------------------------------------------
 * Returns: ( int )     0...grayscale-1 for success, -1=any error
 * --------------------------------------------------------------------------
 * Notes:    o  Handles the eight gridnum's
 *      (gridnum/2 shown to eliminate irrelevant low-order bit)
 *        ---        ---         --*          -*-      
 *        --* = 14   *-- = 19    --* = 42     --* = 73
 *        **-        -**         -*-          --*     
 *
 *        -*-        -**         *--          **-      
 *        *-- = 84   *-- = 112   *-- = 146    --* = 200
 *        *--        ---         -*-          ---     
 * ======================================================================= */
static int aapattern20(raster * rp, int irow, int icol, int gridnum,
                       int grayscale) {
    int aaval = (-1);           /* antialiased value returned */
    int iscenter = gridnum & 1; /* true if pixel at irow,icol black */
    int direction = 1,          /* direction to follow ** line */
        jrow1 = irow, jcol1 = icol,     /* coords of * */
        jrow2 = irow, jcol2 = icol;     /* coords of adjacent ** pixel */
    int turn1 = 0, turn2 = 0, aafollowline();   /* follow *,** lines till turns */
/* -------------------------------------------------------------------------
Initialization and calculation of antialiased value
-------------------------------------------------------------------------- */
/* --- check input -- */
    if (1)
         goto end_of_job;       /* don't want this one */
    if (iscenter)
         goto end_of_job;       /* we only antialias white pixels */
/* --- set params --- */
    switch (gridnum / 2) {      /* check pattern#20 orientation */
    default:
        goto end_of_job;        /* not a pattern#20 gridnum */
        case 14:direction = -2;
        jcol1++;
        jrow2++;
        break;
        case 19:direction = 2;
        jcol1--;
        jrow2++;
        break;
        case 42:direction = 1;
        jrow1++;
        jcol2++;
        break;
        case 73:direction = -1;
        jrow1--;
        jcol2++;
        break;
        case 84:direction = -1;
        jrow1--;
        jcol2--;
        break;
        case 112:direction = 2;
        jcol1--;
        jrow2--;
        break;
        case 146:direction = 1;
        jrow1++;
        jcol2--;
        break;
        case 200:direction = -2;
        jcol1++;
        jrow2--;
        break;
    }                           /* --- end-of-switch(gridnum/2) --- */
/* --- get turns in both directions --- */
        if ((turn1 = aafollowline(rp, jrow1, jcol1, -direction)) == 0)
        goto end_of_job;
    if ((turn2 = aafollowline(rp, jrow2, jcol2, direction)) == 0)
        goto end_of_job;
    if (turn1 * turn2 >= 0)
        goto end_of_job;        /* both turns in same direction */
/* --- weight pixel --- */
    aaval = grayscale / (3 + min2(abs(turn1), abs(turn2)));
/* -------------------------------------------------------------------------
Back to caller with grayscale antialiased value for pixel at irow,icol
-------------------------------------------------------------------------- */
  end_of_job:
    if (aaval >= 0)             /* have antialiasing result */
        if (msglevel >= 99 && msgfp != NULL)
            fprintf(msgfp,      /* diagnostic output */
                    "aapattern20> irow,icol,grid#/2=%d,%d,%d, turn%d,%d=%d,%d, aaval=%d\n",
                    irow, icol, gridnum / 2, -direction, direction, turn1,
                    turn2, aaval);
    return (aaval);             /* back with antialiased value */
}                               /* --- end-of-function aapattern20() --- */


/* ==========================================================================
 * Function:    aapattern39 ( rp, irow, icol, gridnum, grayscale )
 * Purpose: calculates anti-aliased value for pixel at irow,icol,
 *      whose surrounding 3x3 pixel grid is coded by gridnum
 *      (which must correspond to pattern #39).
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster whose bitmap
 *              is to be anti-aliased
 *      irow (I)    int containing row, 0...height-1,
 *              of pixel to be antialiased
 *      icol (I)    int containing col, 0...width-1,
 *              of pixel to be antialiased
 *      gridnum (I) int containing 0...511 corresponding to
 *              3x3 pixel grid surrounding irow,icol
 *      grayscale (I)   int containing number of grayscales
 *              to be calculated, 0...grayscale-1
 *              (should typically be given as 256)
 * --------------------------------------------------------------------------
 * Returns: ( int )     0...grayscale-1 for success, -1=any error
 * --------------------------------------------------------------------------
 * Notes:    o  Handles the eight gridnum's
 *      (gridnum/2 shown to eliminate irrelevant low-order bit)
 *        ---        ---         --*          -**      
 *        --* = 15   *-- = 23    --* = 43     --* = 105
 *        ***        ***         -**          --*     
 *
 *        **-        ***         *--          ***      
 *        *-- = 212  *-- = 240   *-- = 150    --* = 232
 *        *--        ---         **-          ---     
 * ======================================================================= */
static int aapattern39(raster * rp, int irow, int icol, int gridnum,
                       int grayscale) {
    int aaval = (-1);           /* antialiased value returned */
    int iscenter = gridnum & 1; /* true if pixel at irow,icol black */
    int direction = 1,          /* direction to follow ** line */
        jrow1 = irow, jcol1 = icol,     /* coords of * */
        jrow2 = irow, jcol2 = icol;     /* coords of adjacent ** pixel */
    int turn1 = 0, turn2 = 0, aafollowline();   /* follow *,** lines till turns */
/* -------------------------------------------------------------------------
Initialization and calculation of antialiased value
-------------------------------------------------------------------------- */
/* --- check input -- */
    if (iscenter)
         goto end_of_job;       /* we only antialias white pixels */
/* --- set params --- */
    switch (gridnum / 2) {      /* check pattern#39 orientation */
    default:
        goto end_of_job;        /* not a pattern#39 gridnum */
        case 15:direction = -2;
        jcol1++;
        jrow2++;
        break;
        case 23:direction = 2;
        jcol1--;
        jrow2++;
        break;
        case 43:direction = 1;
        jrow1++;
        jcol2++;
        break;
        case 105:direction = -1;
        jrow1--;
        jcol2++;
        break;
        case 212:direction = -1;
        jrow1--;
        jcol2--;
        break;
        case 240:direction = 2;
        jcol1--;
        jrow2--;
        break;
        case 150:direction = 1;
        jrow1++;
        jcol2--;
        break;
        case 232:direction = -2;
        jcol1++;
        jrow2--;
        break;
    }                           /* --- end-of-switch(gridnum/2) --- */
/* --- get turns directions (tunr1==1 signals inside corner) --- */
        if ((turn1 = aafollowline(rp, jrow1, jcol1, -direction)) == 1) {
        aaval = 0;
        goto end_of_job;
    }
    if (1)
        goto end_of_job;        /* stop here for now */
    if ((turn2 = aafollowline(rp, jrow2, jcol2, direction)) == 0)
        goto end_of_job;
    if (turn1 * turn2 >= 0)
        goto end_of_job;        /* both turns in same direction */
/* --- weight pixel --- */
    aaval = grayscale / (3 + min2(abs(turn1), abs(turn2)));
/* -------------------------------------------------------------------------
Back to caller with grayscale antialiased value for pixel at irow,icol
-------------------------------------------------------------------------- */
  end_of_job:
    if (aaval >= 0)             /* have antialiasing result */
        if (msglevel >= 99 && msgfp != NULL)
            fprintf(msgfp,      /* diagnostic output */
                    "aapattern39> irow,icol,grid#/2=%d,%d,%d, turn%d,%d=%d,%d, aaval=%d\n",
                    irow, icol, gridnum / 2, -direction, direction, turn1,
                    turn2, aaval);
    return (aaval);             /* back with antialiased value */
}                               /* --- end-of-function aapattern39() --- */


/* ==========================================================================
 * Function:    aafollowline ( rp, irow, icol, direction )
 * Purpose: starting with pixel at irow,icol, moves in
 *      specified direction looking for a "turn"
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster containing pixel image
 *      irow (I)    int containing row, 0...height-1,
 *              of first pixel
 *      icol (I)    int containing col, 0...width-1,
 *              of first pixel
 *      direction (I)   int containing +1 to follow line up/north
 *              (decreasing irow), -1 to follow line
 *              down/south (increasing irow), +2 to follow
 *              line right/east (increasing icol),
 *              -2 to follow line left/west (decreasing icol)
 * --------------------------------------------------------------------------
 * Returns: ( int )     #rows or #cols traversed prior to turn,
 *              or 0 if no turn detected (or for any error).
 *              Sign is + if turn direction is right/east or
 *              up/north, or is - if left/west or down/south.
 * --------------------------------------------------------------------------
 * Notes:     o Here are some examples illustrating turn detection in
 *      +2 (right/east) direction.  Turns in other directions
 *      are detected similarly/symmetrically.  * denotes black
 *      bits (usually fg), - denotes white bits (usually bg),
 *      and ? denotes "don't care" bit (won't affect outcome).
 *      Arrow --> points to start pixel denoted by irow,icol.
 *
 *         *???         -???    turn=0 (no turn) is returned
 *      -->*???   or -->-???    because the start pixel isn't
 *         *???         -???    on an edge to begin with
 *
 *         ----         **--    turn=0 returned because the
 *      -->***-   or -->***-    line ends abruptly without
 *         ----         ----    turning (even the second case)
 *
 *         ---*         ---*    turn=0 returned because the
 *      -->***-   or -->****    line forms a Y or T rather
 *         ---*         ---*    than turning
 *
 *         ***-         ****    turn=+3 returned
 *      -->***-   or -->***-    (outside corner)
 *         ----         ----
 *
 *         *****        ****-   turn=-4 returned
 *      -->*****  or -->****-   (inside corner)
 *         ----*        ----*
 *
 *         ----*        ----*   turn=+4 returned
 *      -->****-  or -->*****   (outside or inside corner)
 *         -----        -----
 * ======================================================================= */

int aafollowline(raster * rp, int irow, int icol, int direction)
{
    pixbyte *bitmap = rp->pixmap;       /* local rp->pixmap ptr */
    int width = rp->width, height = rp->height; /* width, height of raster */
    int drow = 0, dcol = 0;     /* delta row,col to follow line */
    int jrow = irow, jcol = icol;       /* current row,col following line */
    int bitval = 1;             /* value of rp bit at irow,icol */
    int fgval = 1, bgval = 0;   /* "fg" is whatever bitval is */
    int bitminus = 0, bitplus = 0;      /* value of left/down, right/up bit */
    int isline = 1, isedge = 0; /*isline signals one-pixel wide line */
    int turn = 0;               /* detected turn back to caller */
    int maxturn = maxfollow;    /* don't follow more than max pixels */

    if (irow < 0 || irow >= height || icol < 0 || icol >= width) {
         goto end_of_job;       /* icol out-of-bounds */
    }
/* --- adjust maxturn for magstep --- */
    if (1) {                    /* guard */
        if (magstep > 1 && magstep <= 10) {     /* sanity check */
            maxturn *= magstep; /* factor in magnification */
        }
    }
    /* --- starting bit -- see if we're following a fg (usual), or bg line --- */
    bitval = getlongbit(bitmap, (icol + irow * width));

    /* starting pixel (bg or fg) */
    fgval = bitval;
    bgval = (1 - bitval);       /* define "fg" as whatever bitval is */
/* --- set drow,dcol corresponding to desired direction --- */
    switch (direction) {        /* determine drow,dcol for direction */
    default:
        goto end_of_job;        /* unrecognized direction arg */
    case 1:
        drow = -1;
        break;                  /* follow line up/north */
    case -1:
        drow = 1;
        break;                  /* down/south */
    case 2:
        dcol = 1;
        break;                  /* right/east */
    case -2:
        dcol = -1;
        break;
    }                           /* left/west */

    /* --- set bitminus and bitplus --- */
    if (drow == 0) {
        /* we're following line right/left */
        if (irow < height) {    /* there's a pixel below current */
            bitminus = getlongbit(bitmap, (icol + (irow + 1) * width)); /* get it */
        }
        if (irow > 0) {         /* there's a pixel above current */
            bitplus = getlongbit(bitmap, (icol + (irow - 1) * width));
        }
    }                           /* get it */
    if (dcol == 0) {            /* we're following line up/down */
        if (icol < width) {     /* there's a pixel to the right */
            bitplus = getlongbit(bitmap, (icol + 1 + irow * width));    /* get it */
        }
        if (icol > 0) {         /* there's a pixel to the left */
            bitminus = getlongbit(bitmap, (icol - 1 + irow * width));
        }
    }

    /* --- check for lack of line to follow --- */
    if (bitval == bitplus       /* starting pixel same as above */
        && bitval == bitminus)  /* and below (or right and left) */
        goto end_of_job;        /* so there's no line to follow */
/* --- set isline and isedge (already initted for isline) --- */
    if (bitval == bitplus) {    /* starting pixel same as above */
        isedge = (-1);
        isline = 0;
    }                           /* so we're at an edge below */
    if (bitval == bitminus) {   /* starting pixel same as below */
        isedge = 1;
        isline = 0;
    }
    /* so we're at an edge above */
    /* -------------------------------------------------------------------------
       follow line
       -------------------------------------------------------------------------- */
    while (1) {                 /* until turn found (or max) */
        /* --- local allocations and declarations --- */
        int dbitval = 0,        /* value of bit at jrow,jcol */
            dbitminus = 0, dbitplus = 0;        /* value of left/down, right/up bit */
        /* --- bump pixel count and indexes; check for max or end-of-raster --- */
        turn++;                 /* bump #pixels followed */
        jrow += drow;
        jcol += dcol;           /* indexes of next pixel to check */
        if (turn > maxturn      /* already followed max #pixels */
            || jrow < 0 || jrow >= height       /* or jrow past end-of-raster */
            || jcol < 0 || jcol >= width) {     /* or jcol past end-of-raster */
            turn = 0;
            goto end_of_job;
        }
        /* so quit without finding a turn */
        /* --- set current bit (dbitval) --- */
        dbitval = getlongbit(bitmap, (jcol + jrow * width));    /*value of jrow,jcol bit */
        /* --- set dbitminus and dbitplus --- */
        if (drow == 0) {        /* we're following line right/left */
            if (irow < height) {  /* there's a pixel below current */
                dbitminus = getlongbit(bitmap, (jcol + (irow + 1) * width));
            }
            if (irow > 0) {      /* there's a pixel above current */
                dbitplus = getlongbit(bitmap, (jcol + (irow - 1) * width));
            }
        }                       /* get it */
        if (dcol == 0) {        /* we're following line up/down */
            if (icol < width) {  /* there's a pixel to the right */
                dbitplus = getlongbit(bitmap, (icol + 1 + jrow * width));
            }
            if (icol > 0) {      /* there's a pixel to the left */
                dbitminus = getlongbit(bitmap, (icol - 1 + jrow * width));
            }
        }
        /* get it */
        /* --- first check for abrupt end-of-line, or for T or Y --- */
        if (isline != 0)        /* abrupt end or T,Y must be a line */
            if ((bgval == dbitval       /* end-of-line if pixel flips to bg */
                 && bgval == dbitplus   /* and bg same as above pixel */
                 && bgval == dbitminus) /* and below (or right and left) */
                ||(fgval == dbitplus    /* T or Y if fg same as above pixel */
                   && fgval == dbitminus)) {    /* and below (or right and left) */
                turn = 0;
                goto end_of_job;
            }
        /* so we're at a T or Y */
        /* --- check for turning line --- */
        if (isline != 0) {      /* turning line must be a line */
            if (fgval == dbitminus) {   /* turning down */
                turn = -turn;
                goto end_of_job;
            } /* so return negative turn */
            else if (fgval == dbitplus) /* turning up */
                goto end_of_job;
        }
        /* so return positive turn */
        /* --- check for inside corner at edge --- */
        if (isedge != 0) {      /* inside corner must be a edge */
            if (isedge < 0 && fgval == bitminus) {      /* corner below */
                turn = -turn;
                goto end_of_job;
            }                   /* so return negative turn */
            if (isedge > 0 && fgval == bitplus) { /* corner above */
                goto end_of_job;
            }
        }
        /* so return positive turn */
        /* --- check for abrupt end at edge --- */
        if (isedge != 0         /* abrupt edge end must be an edge */
            && fgval == dbitval)        /* and line must not end */
            if ((isedge < 0 && bgval == bitplus)        /* abrupt end above */
                ||(isedge > 0 && bgval == bitminus)) {  /* or abrupt end below */
                turn = 0;
                goto end_of_job;
            }
        /* so edge ended abruptly */
        /* --- check for outside corner at edge --- */
        if (isedge != 0         /* outside corner must be a edge */
            && bgval == dbitval) {      /* and line must end */
            if (isedge > 0)
                turn = -turn;   /* outside turn down from edge above */
            goto end_of_job;
        }
    }

/* -------------------------------------------------------------------------
Back to caller with #rows or #cols traversed, and direction of detected turn
-------------------------------------------------------------------------- */
  end_of_job:
    if (msglevel >= 99 && msgfp != NULL) {
        fprintf(msgfp,
                "aafollowline> irow,icol,direction=%d,%d,%d, turn=%d\n",
                irow, icol, direction, turn);
    }
    return turn;
}

/* ==========================================================================
 * Function:    aagridnum ( rp, irow, icol )
 * Purpose: calculates gridnum, 0-511 (see Notes below),
 *      for 3x3 grid centered at irow,icol
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster containing
 *              bitmap image (to be anti-aliased)
 *      irow (I)    int containing row, 0...height-1,
 *              at center of 3x3 grid
 *      icol (I)    int containing col, 0...width-1,
 *              at center of 3x3 grid
 * --------------------------------------------------------------------------
 * Returns: ( int )     0-511 grid number, or -1=any error
 * --------------------------------------------------------------------------
 * Notes:     o Input gridnum is a 9-bit int, 0-511, coding a 3x3 pixel grid
 *      whose bit positions (and corresponding values) in gridnum are
 *        876     256 128  64
 *        504  =   32   1  16
 *        321       8   4   2
 *      Thus, for example (*=pixel set/black, -=pixel not set/white),
 *        *--         *--     -**         (note that 209 is the
 *        -*- = 259   *-- = 302   -** = 209    inverse, set<-->unset,
 *        --*         ***         ---          of 302)
 *        o A set pixel is considered black, an unset pixel considered
 *      white.
 * ======================================================================= */

static int aagridnum(raster * rp, int irow, int icol)
{
    pixbyte *bitmap = rp->pixmap;       /* local rp->pixmap ptr */
    int width = rp->width, height = rp->height; /* width, height of raster */
    int imap = icol + irow * width;     /* pixel index = icol + irow*width */
    int bitval = 0;             /* value of rp bit at irow,icol */
    int nnbitval = 0, nebitval = 0, eebitval = 0, sebitval = 0; /*adjacent vals */
    int ssbitval = 0, swbitval = 0, wwbitval = 0, nwbitval = 0; /*compass pt names */
    int gridnum = -1;           /* grid# 0-511 for above 9 bits */

    if (irow < 0 || irow >= height|| icol < 0 || icol >= width) {
         goto end_of_job;       /* icol out-of-bounds */
    }

/* -------------------------------------------------------------------------
get the 9 bits comprising the 3x3 grid centered at irow,icol
-------------------------------------------------------------------------- */
/* --- get center bit --- */
     bitval = getlongbit(bitmap, imap); /* value of rp input bit at imap */
/* --- get 8 surrounding bits --- */
    if (irow > 0) {              /* nn (north) bit available */
         nnbitval = getlongbit(bitmap, imap - width);   /* nn bit value */
    }
    if (irow < height - 1) {     /* ss (south) bit available */
         ssbitval = getlongbit(bitmap, imap + width);   /* ss bit value */
    }
    if (icol > 0) {             /* ww (west) bit available */
        wwbitval = getlongbit(bitmap, imap - 1);        /* ww bit value */
        if (irow > 0) {          /* nw bit available */
            nwbitval = getlongbit(bitmap, imap - width - 1);    /* nw bit value */
        }
        if (irow < height - 1) { /* sw bit available */
            swbitval = getlongbit(bitmap, imap + width - 1);
        }
    } if (icol < width - 1) {        /* ee (east) bit available */
        eebitval = getlongbit(bitmap, imap + 1);        /* ee bit value */
        if (irow > 0) {          /* ne bit available */
            nebitval = getlongbit(bitmap, imap - width + 1);    /* ne bit value */
        }
        if (irow < height - 1) { /* se bit available */
            sebitval = getlongbit(bitmap, imap + width + 1);
        }
    }

    /* --- set gridnum --- */
    gridnum = 0;                /* clear all bits */
    if (bitval)
        gridnum = 1;            /* set1bit(gridnum,0); */
    if (nwbitval)
        gridnum += 256;         /* set1bit(gridnum,8); */
    if (nnbitval)
        gridnum += 128;         /* set1bit(gridnum,7); */
    if (nebitval)
        gridnum += 64;          /* set1bit(gridnum,6); */
    if (wwbitval)
        gridnum += 32;          /* set1bit(gridnum,5); */
    if (eebitval)
        gridnum += 16;          /* set1bit(gridnum,4); */
    if (swbitval)
        gridnum += 8;           /* set1bit(gridnum,3); */
    if (ssbitval)
        gridnum += 4;           /* set1bit(gridnum,2); */
    if (sebitval)
        gridnum += 2;           /* set1bit(gridnum,1); */
/* -------------------------------------------------------------------------
Back to caller with gridnum coding 3x3 grid centered at irow,icol
-------------------------------------------------------------------------- */
  end_of_job:
    return (gridnum);
}                               /* --- end-of-function aagridnum() --- */


/* ==========================================================================
 * Function:    aapatternnum ( gridnum )
 * Purpose: Looks up the pattern number 1...51
 *      corresponding to the 3x3 pixel grid coded by gridnum 0=no
 *      pixels set (white) to 511=all pixels set (black).
 * --------------------------------------------------------------------------
 * Arguments:   gridnum (I) int containing 0-511 coding a 3x3 pixel grid
 *              (see Notes below)
 * --------------------------------------------------------------------------
 * Returns: ( int )     1 to 51, or -1=error
 * --------------------------------------------------------------------------
 * Notes:     o Input gridnum is a 9-bit int, 0-511, coding a 3x3 pixel grid
 *      whose bit positions (and corresponding values) in gridnum are
 *        876     256 128  64
 *        504  =   32   1  16
 *        321       8   4   2
 *      Thus, for example (*=pixel set/black, -=pixel not set/white),
 *        *--         *--     -**         (note that 209 is the
 *        -*- = 259   *-- = 302   -** = 209    inverse, set<-->unset,
 *        --*         ***         ---          of 302)
 *        o A set pixel is considered black, an unset pixel considered
 *      white.
 *        o Ignoring whether the center pixel is set or unset, and
 *      taking rotation, reflection and inversion (set<-->unset)
 *      symmetries into account, there are 32 unique pixel patterns.
 *      If inversions are listed separately, there are 51 patterns.
 *        o Here are the 51 unique patterns, with ? always denoting the
 *      undetermined center pixel.  At the upper-left corner of each
 *      pattern is the "pattern index number" assigned to it in this
 *      function. At the upper-right is the pattern's multiplicity,
 *      i.e., the number of different patterns obtained by rotations
 *      and reflection of the illustrated one.  Inverse patters are
 *      illustrated immediately beneath the original (the first three
 *      four-pixel patterns have identical inverses).
 *      -------------------------------------------------------------
 *      No pixels set:
 *       #1 1 (in this case, 1 signifies that rotation
 *        ---  and reflection give no different grids)
 *        -?-
 *        ---
 *      Inverse, all eight pixels set
 *       #2 1 (the inverse multiplicity is always the same)
 *        ***
 *        *?*
 *        ***
 *      -------------------------------------------------------------
 *      One pixel set:
 *       #3 4  #4 4
 *        *--   -*-
 *        -?-   -?-
 *        ---   ---
 *      Inverse, seven pixels set:
 *       #5 4  #6 4
 *        -**   *-*
 *        *?*   *?*
 *        ***   ***
 *      -------------------------------------------------------------
 *      Two pixels set:
 *       #7 8  #8 4  #9 8  10 2  11 4  12 2
 *        **-   *-*   *--   *--   -*-   -*-
 *        -?-   -?-   -?*   -?-   -?*   -?-
 *        ---   ---   ---   --*   ---   -*-
 *      Inverse, six pixels set:
 *      #13 8  14 4  15 8  16 2  17 4  18 2
 *        --*   -*-   -**   -**   *-*   *-*
 *        *?*   *?*   *?-   *?*   *?-   *?*
 *        ***   ***   ***   **-   ***   *-*
 *      -------------------------------------------------------------
 *      Three pixels set:
 *      #19 4  20 8  21 8  22 8  23 8  24 4  25 4  26 4  27 4  28 4
 *        ***   **-   **-   **-   **-   **-   *-*   *-*   -*-   -*-
 *        -?-   -?*   -?-   -?-   -?-   *?-   -?-   -?-   -?*   -?*
 *        ---   ---   --*   -*-   *--   ---   --*   -*-   -*-   *--
 *      Inverse, five pixels set:
 *      #29 4  30 8  31 8  32 8  33 8  34 4  35 4  36 4  37 4  38 4
 *        ---   --*   --*   --*   --*   --*   -*-   -*-   *-*   *-*
 *        *?*   *?-   *?*   *?*   *?*   -?*   *?*   *?*   *?-   *?-
 *        ***   ***   **-   *-*   -**   ***   **-   *-*   *-*   -**
 *      -------------------------------------------------------------
 *      Four pixels set (including inverses):
 *      #39 8  40 4  41 8  42 8  43 4  44 4  45 8  46 1
 *        ***   **-   **-   ***   ***   **-   **-   *-*
 *        -?*   -?-   -?*   -?-   -?-   -?*   -?*   -?-
 *        ---   -**   *--   --*   -*-   --*   -*-   *-*
 *
 *                        #47 8  48 4  49 4  50 8  51 1
 *                          ---   ---   --*   --*   -*-
 *                          *?*   *?*   *?-   *?-   *?*
 *                          **-   *-*   **-   *-*   -*-
 * ======================================================================= */
static int aapatternnum(int gridnum) {
    int pattern = (-1);         /*pattern#, 1-51, for input gridnum */
/* ---
 * pattern number corresponding to input gridnum/2 code
 * ( gridnum/2 strips off gridnum's low bit because it's
 * the same pattern whether or not center pixel is set )
 * --- */
    static int patternnum[] = {
        1, 3, 4, 7, 3, 8, 7, 19, 4, 7, 11, 24, 9, 23, 20, 39,   /*   0- 15 */
            4, 9, 11, 20, 7, 23, 24, 39, 12, 22, 27, 47, 22, 48, 47, 29,        /*  16- 31 */
            3, 8, 9, 23, 10, 25, 21, 42, 7, 19, 20, 39, 21, 42, 44, 34, /*  32- 47 */
            9, 26, 28, 41, 21, 50, 49, 30, 22, 43, 45, 33, 40, 32, 31, 13,      /*  48- 63 */
            4, 9, 12, 22, 9, 26, 22, 43, 11, 20, 27, 47, 28, 41, 45, 33,        /*  64- 79 */
            11, 28, 27, 45, 20, 41, 47, 33, 27, 45, 51, 35, 45, 36, 35, 14,     /*  80- 95 */
            7, 23, 22, 48, 21, 50, 40, 32, 24, 39, 47, 29, 49, 30, 31, 13,      /*  96-111 */
            20, 41, 45, 36, 44, 38, 31, 15, 47, 33, 35, 14, 31, 15, 16, 5,      /* 112-127 */
            3, 10, 9, 21, 8, 25, 23, 42, 9, 21, 28, 49, 26, 50, 41, 30, /* 128-143 */
            7, 21, 20, 44, 19, 42, 39, 34, 22, 40, 45, 31, 43, 32, 33, 13,      /* 144-159 */
            8, 25, 26, 50, 25, 46, 50, 37, 23, 42, 41, 30, 50, 37, 38, 17,      /* 160-175 */
            23, 50, 41, 38, 42, 37, 30, 17, 48, 32, 36, 15, 32, 18, 15, 6,      /* 176-191 */
            7, 21, 22, 40, 23, 50, 48, 32, 20, 44, 45, 31, 41, 38, 36, 15,      /* 192-207 */
            24, 49, 47, 31, 39, 30, 29, 13, 47, 31, 35, 16, 33, 15, 14, 5,      /* 208-223 */
            19, 42, 43, 32, 42, 37, 32, 18, 39, 34, 33, 13, 30, 17, 15, 6,      /* 224-239 */
            39, 30, 33, 15, 34, 17, 13, 6, 29, 13, 14, 5, 13, 6, 5, 2,  /* 240-255 */
    -1};                        /* --- end-of-patternnum[] --- */
/* -------------------------------------------------------------------------
look up pattern number for gridnum
-------------------------------------------------------------------------- */
/* --- first check input --- */
    if (gridnum < 0 || gridnum > 511)
        goto end_of_job;        /* gridnum out of bounds */
/* --- look up pattern number, 1-51, corresponding to input gridnum --- */
    pattern = patternnum[gridnum / 2];  /* /2 strips off gridnum's low bit */
    if (pattern < 1 || pattern > 51)
        pattern = (-1);         /* some internal error */
  end_of_job:
    return (pattern);           /* back to caller with pattern# */
}                               /* --- end-of-function aapatternnum() --- */


/* ==========================================================================
 * Function:    aalookup ( gridnum )
 * Purpose: Looks up the grayscale value 0=white to 255=black
 *      corresponding to the 3x3 pixel grid coded by gridnum 0=no
 *      pixels set (white) to 511=all pixels set (black).
 * --------------------------------------------------------------------------
 * Arguments:   gridnum (I) int containing 0-511 coding a 3x3 pixel grid
 * --------------------------------------------------------------------------
 * Returns: ( int )     0=white to 255=black, or -1=error
 * --------------------------------------------------------------------------
 * Notes:     o Input gridnum is a 9-bit int, 0-511, coding a 3x3 pixel grid
 *        o A set pixel is considered black, an unset pixel considered
 *      white.  Likewise, the returned grayscale is 255 for black,
 *      0 for white.  You'd more typically want to use 255-grayscale
 *      so that 255 is white and 0 is black.
 *        o The returned number is the (lowpass) antialiased grayscale
 *      for the center pixel (gridnum bit 0) of the grid.
 * ======================================================================= */
static int aalookup(int gridnum) {
    int grayscale = (-1);       /*returned grayscale, init for error */
    int pattern = (-1), aapatternnum(); /*pattern#, 1-51, for input gridnum */
    int iscenter = gridnum & 1; /*low-order bit set for center pixel */
/* --- gray scales --- */
#define WHT 0
#define LGT 64
#define GRY 128
#define DRK 192
#define BLK 255
#if 1
/* ---
 * modified aapnm() grayscales (second try)
 * --- */
/* --- grayscale for each pattern when center pixel set/black --- */
    static int grayscale1[] = {
        -1,                     /* [0] index not used */
            BLK, BLK, BLK, BLK, 242, 230, BLK, BLK, BLK, 160,   /*  1-10 */
            /* BLK,BLK,BLK,BLK,242,230,BLK,BLK,BLK,BLK, *//*  1-10 */
            BLK, BLK, 217, 230, 217, 230, 204, BLK, BLK, 166,   /* 11-20 */
            BLK, BLK, BLK, BLK, BLK, BLK, 178, 166, 204, 191,   /* 21-30 */
            204, BLK, 204, 191, 217, 204, BLK, 191, 178, BLK,   /* 31-40 */
            178, BLK, BLK, 178, 191, BLK, 191, BLK, 178, BLK, 204,      /* 41-51 */
    -1};                        /* --- end-of-grayscale1[] --- */
/* --- grayscale for each pattern when center pixel not set/white --- */
    static int grayscale0[] = {
        -1,                     /* [0] index not used */
            WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT,   /*  1-10 */
            64, WHT, WHT, 128, 115, 128, WHT, WHT, WHT, 64,     /* 11-20 */
            /* 51,WHT,WHT,128,115,128,WHT,WHT,WHT, 64, *//* 11-20 */
            WHT, WHT, WHT, 64, WHT, WHT, 76, 64, 102, 89,       /* 21-30 */
            102, WHT, 102, WHT, 115, 102, WHT, 89, 76, WHT,     /* 31-40 */
            76, WHT, WHT, 76, 89, WHT, 89, WHT, 76, WHT, 102,   /* 41-51 */
    -1};                        /* --- end-of-grayscale0[] --- */
#endif
#if 0
/* ---
 * modified aapnm() grayscales (first try)
 * --- */
/* --- grayscale for each pattern when center pixel set/black --- */
    static int grayscale1[] = {
        -1,                     /* [0] index not used */
            BLK, BLK, BLK, BLK, 242, 230, GRY, BLK, BLK, BLK,   /*  1-10 */
            /* BLK,BLK,BLK,BLK,242,230,BLK,BLK,BLK,BLK, *//*  1-10 */
            BLK, BLK, 217, 230, 217, 230, 204, BLK, BLK, 166,   /* 11-20 */
            BLK, BLK, BLK, BLK, BLK, BLK, BLK, 166, 204, 191,   /* 21-30 */
            /* BLK,BLK,BLK,BLK,BLK,BLK,178,166,204,191, *//* 21-30 */
            204, BLK, 204, BLK, 217, 204, BLK, 191, GRY, BLK,   /* 31-40 */
            /* 204,BLK,204,191,217,204,BLK,191,178,BLK, *//* 31-40 */
            178, BLK, BLK, 178, 191, BLK, BLK, BLK, 178, BLK, 204,      /* 41-51 */
            /* 178,BLK,BLK,178,191,BLK,191,BLK,178,BLK,204, *//* 41-51 */
    -1};                        /* --- end-of-grayscale1[] --- */
/* --- grayscale for each pattern when center pixel not set/white --- */
    static int grayscale0[] = {
        -1,                     /* [0] index not used */
            WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT,   /*  1-10 */
            GRY, WHT, WHT, 128, 115, 128, WHT, WHT, WHT, GRY,   /* 11-20 */
            /*  51,WHT,WHT,128,115,128,WHT,WHT,WHT, 64, *//* 11-20 */
            WHT, WHT, WHT, GRY, WHT, WHT, 76, 64, 102, 89,      /* 21-30 */
            /* WHT,WHT,WHT, 64,WHT,WHT, 76, 64,102, 89, *//* 21-30 */
            102, WHT, 102, WHT, 115, 102, WHT, 89, GRY, WHT,    /* 31-40 */
            /* 102,WHT,102,WHT,115,102,WHT, 89, 76,WHT, *//* 31-40 */
            76, WHT, WHT, GRY, 89, WHT, 89, WHT, 76, WHT, 102,  /* 41-51 */
            /*  76,WHT,WHT, 76, 89,WHT, 89,WHT, 76,WHT,102, *//* 41-51 */
    -1};                        /* --- end-of-grayscale0[] --- */
#endif
#if 0
/* ---
 * these grayscales _exactly_ correspond to the aapnm() algorithm
 * --- */
/* --- grayscale for each pattern when center pixel set/black --- */
    static int grayscale1[] = {
        -1,                     /* [0] index not used */
            BLK, BLK, BLK, BLK, 242, 230, BLK, BLK, BLK, BLK,   /*  1-10 */
            BLK, BLK, 217, 230, 217, 230, 204, BLK, BLK, 166,   /* 11-20 */
            BLK, BLK, BLK, BLK, BLK, BLK, 178, 166, 204, 191,   /* 21-30 */
            204, BLK, 204, 191, 217, 204, BLK, 191, 178, BLK,   /* 31-40 */
            178, BLK, BLK, 178, 191, BLK, 191, BLK, 178, BLK, 204,      /* 41-51 */
    -1};                        /* --- end-of-grayscale1[] --- */
/* --- grayscale for each pattern when center pixel not set/white --- */
    static int grayscale0[] = {
        -1,                     /* [0] index not used */
            WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT,   /*  1-10 */
            51, WHT, WHT, 128, 115, 128, WHT, WHT, WHT, 64,     /* 11-20 */
            WHT, WHT, WHT, 64, WHT, WHT, 76, 64, 102, 89,       /* 21-30 */
            102, WHT, 102, WHT, 115, 102, WHT, 89, 76, WHT,     /* 31-40 */
            76, WHT, WHT, 76, 89, WHT, 89, WHT, 76, WHT, 102,   /* 41-51 */
    -1};                        /* --- end-of-grayscale0[] --- */
#endif
/* -------------------------------------------------------------------------
look up grayscale for gridnum
-------------------------------------------------------------------------- */
/* --- first check input --- */
    if (gridnum < 0 || gridnum > 511)
        goto end_of_job;        /* gridnum out of bounds */
/* --- look up pattern number, 1-51, corresponding to input gridnum --- */
    pattern = aapatternnum(gridnum);    /* look up pattern number */
    if (pattern < 1 || pattern > 51)
        goto end_of_job;        /* some internal error */
    if (ispatternnumcount) {    /* counts being accumulated */
        if (iscenter)
            patternnumcount1[pattern] += 1;     /* bump diagnostic count */
        else
            patternnumcount0[pattern] += 1;
    }
/* --- look up grayscale for this pattern --- */
    grayscale = (iscenter ? grayscale1[pattern] : grayscale0[pattern]);
  end_of_job:
    return grayscale;
}


/* ==========================================================================
 * Function:    aalowpasslookup ( rp, bytemap, grayscale )
 * Purpose: calls aalookup() for each pixel in rp->bitmap
 *      to create anti-aliased bytemap
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster whose bitmap
 *              is to be anti-aliased
 *      bytemap (O) intbyte * to bytemap, calculated
 *              by calling aalookup() for each pixel
 *              in rp->bitmap
 *      grayscale (I)   int containing number of grayscales
 *              to be calculated, 0...grayscale-1
 *              (should typically be given as 256)
 * --------------------------------------------------------------------------
 * Returns: ( int )     1=success, 0=any error
 * --------------------------------------------------------------------------
 * Notes:    o
 * ======================================================================= */
int aalowpasslookup(raster * rp, intbyte * bytemap, int grayscale) {
    int width = rp->width, height = rp->height, /* width, height of raster */
        icol = 0, irow = 0, imap = (-1);        /* width, height, bitmap indexes */
    int bgbitval = 0 /*, fgbitval=1 */ ;        /* background, foreground bitval */
    int bitval = 0,             /* value of rp bit at irow,icol */
        aabyteval = 0;          /* antialiased (or unchanged) value */
    int gridnum = 0, aagridnum(),       /* grid# for 3x3 grid at irow,icol */
        aalookup();             /* table look up  antialiased value */
/* -------------------------------------------------------------------------
generate bytemap by table lookup for each pixel of bitmap
-------------------------------------------------------------------------- */
    for (irow = 0; irow < height; irow++)
        for (icol = 0; icol < width; icol++) {
            /* --- get gridnum and center bit value, init aabyteval --- */
            gridnum = aagridnum(rp, irow, icol);        /*grid# coding 3x3 grid at irow,icol */
            bitval = (gridnum & 1);     /* center bit set if gridnum odd */
            aabyteval = (intbyte) (bitval == bgbitval ? 0 : grayscale - 1);     /* default aa val */
            imap++;             /* first bump bitmap[] index */
            bytemap[imap] = (intbyte) (aabyteval);      /* init antialiased pixel */
            /* --- look up antialiased value for this grid --- */
            aabyteval = aalookup(gridnum);      /* look up on grid# */
            if (aabyteval >= 0 && aabyteval <= 255)     /* check for success */
                bytemap[imap] = (intbyte) (aabyteval);  /* init antialiased pixel */
        }                       /* --- end-of-for(irow,icol) --- */
    ispatternnumcount = 0;      /* accumulate counts only once */

/*end_of_job:*/
    return 1;
}

/* ==========================================================================
 * Function:    aasupsamp ( rp, aa, sf, grayscale )
 * Purpose: calculates a supersampled anti-aliased bytemap
 *      for rp->bitmap, with each byte 0...grayscale-1
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster whose bitmap
 *              is to be anti-aliased
 *      aa (O)      address of raster * to supersampled bytemap,
 *              calculated by supersampling rp->bitmap
 *      sf (I)      int containing supersampling shrinkfactor
 *      grayscale (I)   int containing number of grayscales
 *              to be calculated, 0...grayscale-1
 *              (should typically be given as 256)
 * --------------------------------------------------------------------------
 * Returns: ( int )     1=success, 0=any error
 * --------------------------------------------------------------------------
 * Notes:     o If the center point of the box being averaged is black,
 *      then the entire "average" is forced black (grayscale-1)
 *      regardless of the surrounding points.  This is my attempt
 *      to avoid a "washed-out" appearance of thin (one-pixel-wide)
 *      lines, which would otherwise turn from black to a gray shade.
 * ======================================================================= */
static int aasupsamp(raster * rp, raster ** aa, int sf, int grayscale)
{
    int status = 0;             /* 1=success, 0=failure to caller */
    int rpheight = rp->height, rpwidth = rp->width,     /*bitmap raster dimensions */
        heightrem = 0, widthrem = 0,    /* rp+rem is a multiple of shrinkf */
        aaheight = 0, aawidth = 0,      /* supersampled dimensions */
        aapixsz = 8;            /* output pixels are 8-bit bytes */
    int maxaaval = -9999,       /* max grayscale val set in matrix */
        isrescalemax = 1;       /* 1=rescale maxaaval to grayscale */
    int irp = 0, jrp = 0, iaa = 0, jaa = 0, iwt = 0, jwt = 0;   /*indexes, i=width j=height */
    raster *aap = NULL;         /* raster for supersampled image */
    static raster *aawts = NULL;        /* aaweights() resultant matrix */
    static int prevshrink = NOVALUE,    /* shrinkfactor from previous call */
        sumwts = 0;             /* sum of weights */
    static int blackfrac = 40,  /* force black if this many pts are */
        /*grayfrac = 20, */
        maxwt = 10,             /* max weight in weight matrix */
        minwtfrac = 10, maxwtfrac = 70; /* force light pts white, dark black */

/* --- check args --- */
    if (aa == NULL)
        goto end_of_job;        /* no ptr for return output arg */
    *aa = NULL;                 /* init null ptr for error return */
    if (rp == NULL              /* no ptr to input arg */
        || sf < 1               /* invalid shrink factor */
        || grayscale < 2)
        goto end_of_job;        /* invalid grayscale */
/* --- get weight matrix (or use current one) --- */
    if (sf != prevshrink) {     /* have new shrink factor */
        if (aawts != NULL)      /* have unneeded weight matrix */
            delete_raster(aawts);       /* so free it */
        sumwts = 0;             /* reinitialize sum of weights */
        aawts = aaweights(sf, sf);      /* get new weight matrix */
        if (aawts != NULL)      /* got weight matrix okay */
            for (jwt = 0; jwt < sf; jwt++)      /* for each row */
                for (iwt = 0; iwt < sf; iwt++) {        /* and each column */
                    int wt = (int) (getpixel(aawts, jwt, iwt)); /* weight */
                    if (wt > maxwt) {   /* don't overweight center pts */
                        wt = maxwt;     /* scale it back */
                        setpixel(aawts, jwt, iwt, wt);
                    } /* and replace it in matrix */ sumwts += wt;
                }               /* add weight to sum */
        prevshrink = sf;
    }                           /* save new shrink factor */
    if (msgfp != NULL && msglevel >= 999) {
        fprintf(msgfp, "aasupsamp> sf=%d, sumwts=%d weights=...\n", sf,
                sumwts);
        type_bytemap((intbyte *) aawts->pixmap, grayscale, aawts->width,
                     aawts->height, msgfp);
    }
/* --- calculate supersampled height,width and allocate output raster */
    heightrem = rpheight % sf;  /* remainder after division... */
    widthrem = rpwidth % sf;    /* ...by shrinkfactor */
    aaheight = 1 + (rpheight + sf - (heightrem + 1)) / sf;      /* ss height */
    aawidth = 1 + (rpwidth + sf - (widthrem + 1)) / sf; /* ss width */
    if (msgfp != NULL && msglevel >= 999) {
        fprintf(msgfp,
                "aasupsamp> rpwid,ht=%d,%d wd,htrem=%d,%d aawid,ht=%d,%d\n",
                rpwidth, rpheight, widthrem, heightrem, aawidth, aaheight);
        fprintf(msgfp, "aasupsamp> dump of original bitmap image...\n");
        type_raster(rp, msgfp);
    }                           /* ascii image of rp raster */
    if ((aap = new_raster(aawidth, aaheight, aapixsz)) == NULL) {
        goto end_of_job;        /* quit if alloc fails */
    }
/* -------------------------------------------------------------------------
Step through rp->bitmap, applying aawts to each "submatrix" of bitmap
-------------------------------------------------------------------------- */
    for (jaa = 0, jrp = (-(heightrem + 1) / 2); jrp < rpheight; jrp += sf) {    /* height */
        for (iaa = 0, irp = (-(widthrem + 1) / 2); irp < rpwidth; irp += sf) {  /* width */
            int aaval = 0;      /* weighted rpvals */
            int nrp = 0, mrp = 0;       /* #rp bits set, #within matrix */
            for (jwt = 0; jwt < sf; jwt++)
                for (iwt = 0; iwt < sf; iwt++) {
                    int i = irp + iwt, j = jrp + jwt;   /* rp->pixmap point */
                    int rpval = 0;      /* rp->pixmap value at i,j */
                    if (i >= 0 && i < rpwidth   /* i within actual pixmap */
                        && j >= 0 && j < rpheight) {    /* ditto j */
                        mrp++;  /* count another bit within matrix */
                        rpval = (int) (getpixel(rp, j, i));
                    }           /* get actual pixel value */
                    if (rpval != 0) {
                        nrp++;  /* count another bit set */
                        aaval += (int) (getpixel(aawts, jwt, iwt));
                    }           /* sum weighted vals */
                }               /* --- end-of-for(iwt,jwt) --- */
            if (aaval > 0) {    /*normalize and rescale non-zero val */
                int aafrac = (100 * aaval) / sumwts;    /* weighted percent of black points */
                /*if((100*nrp)/mrp >=blackfrac) *//* many black interior pts */
                if (aafrac >= maxwtfrac)        /* high weight of sampledblack pts */
                    aaval = grayscale - 1;      /* so set supersampled pt black */
                else if (aafrac <= minwtfrac)   /* low weight of sampledblack pts */
                    aaval = 0;  /* so set supersampled pt white */
                else            /* rescale calculated weight */
                    aaval =
                        ((sumwts / 2 - 1) +
                         (grayscale - 1) * aaval) / sumwts;
            }
            maxaaval = max2(maxaaval, aaval);   /* largest aaval so far */
            if (msgfp != NULL && msglevel >= 999)
                fprintf(msgfp, "aasupsamp> jrp,irp=%d,%d jaa,iaa=%d,%d"
                        " mrp,nrp=%d,%d aaval=%d\n",
                        jrp, irp, jaa, iaa, mrp, nrp, aaval);
            if (jaa < aaheight && iaa < aawidth)        /* bounds check */
                setpixel(aap, jaa, iaa, aaval); /*weighted val in supersamp raster */
            else if (msgfp != NULL && msglevel >= 9)    /* emit error if out-of-bounds */
                fprintf(msgfp,
                        "aasupsamp> Error: aaheight,aawidth=%d,%d jaa,iaa=%d,%d\n",
                        aaheight, aawidth, jaa, iaa);
            iaa++;              /* bump aa col index */
        }                       /* --- end-of-for(irp) --- */
        jaa++;                  /* bump aa row index */
    }                           /* --- end-of-for(jrp) --- */
/* --- rescale supersampled image so darkest points become black --- */
    if (isrescalemax) {         /* flag set to rescale maxaaval */
        double scalef = ((double) (grayscale - 1)) / ((double) maxaaval);
        for (jaa = 0; jaa < aaheight; jaa++)    /* height */
            for (iaa = 0; iaa < aawidth; iaa++) {       /* width */
                int aafrac, aaval = getpixel(aap, jaa, iaa);    /* un-rescaled value */
                aaval = (int) (0.5 + ((double) aaval) * scalef);        /*multiply by scale factor */
                aafrac = (100 * aaval) / (grayscale - 1);       /* fraction of blackness */
                if (aafrac >= blackfrac)        /* high weight of sampledblack pts */
                    aaval = grayscale - 1;      /* so set supersampled pt black */
                else if (0 && aafrac <= minwtfrac)      /* low weight of sampledblack pts */
                    aaval = 0;  /* so set supersampled pt white */
                setpixel(aap, jaa, iaa, aaval);
            }                   /* replace rescaled val in raster */
    }                           /* --- end-of-if(isrescalemax) --- */
    *aa = aap;                  /* return supersampled image */
    status = 1;                 /* set successful status */
    if (msgfp != NULL && msglevel >= 999) {
        fprintf(msgfp, "aasupsamp> anti-aliased image...\n");
        type_bytemap((intbyte *) aap->pixmap, grayscale,
                     aap->width, aap->height, msgfp);
        fflush(msgfp);
    }
  end_of_job:
    return (status);
}

#if 0
/* ==========================================================================
 * Function:    aacolormap ( bytemap, nbytes, colors, colormap )
 * Purpose: searches bytemap, returning a list of its discrete values
 *      in ascending order in colors[], and returning an "image"
 *      of bytemap (where values are replaced by colors[]
 *      indexes) in colormap[].
 * --------------------------------------------------------------------------
 * Arguments:   bytemap (I) intbyte *  to bytemap containing
 *              grayscale values (usually 0=white
 *              through 255=black) for which colors[]
 *              and colormap[] will be constructed.
 *      nbytes (I)  int containing #bytes in bytemap
 *              (usually just #rows * #cols)
 *      colors (O)  intbyte *  (to be interpreted as ints)
 *              returning a list of the discrete/different
 *              values in bytemap, in ascending value order,
 *              and with gamma correction applied
 *      colormap (O)    intbyte *  returning a bytemap "image",
 *              i.e., in one-to-one pixel correspondence
 *              with bytemap, but where the values have been
 *              replaced with corresponding colors[] indexes.
 * --------------------------------------------------------------------------
 * Returns: ( int )     #colors in colors[], or 0 for any error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static int aacolormap(intbyte * bytemap, int nbytes, intbyte * colors,
                      intbyte * colormap) {
    int ncolors = 0,            /* #different values in bytemap */
        igray, grayscale = 256; /* bytemap contains intbyte's */
    intbyte *bytevalues = NULL; /* 1's where bytemap contains value */
    int ibyte;                  /* bytemap/colormap index */
    int isscale = 0,            /* true to scale largest val to 255 */
        isgamma = 1;            /* true to apply gamma correction */
    int maxcolors = 0;          /* maximum ncolors */
/* -------------------------------------------------------------------------
Accumulate colors[] from values occurring in bytemap
-------------------------------------------------------------------------- */
/* --- initialization --- */
    if ((bytevalues = (intbyte *) malloc(grayscale))    /*alloc bytevalues */
        == NULL)
        goto end_of_job;        /* signal error if malloc() failed */
     memset(bytevalues, 0, grayscale);  /* zero out bytevalues */
/* --- now set 1's at offsets corresponding to values found in bytemap --- */
    for (ibyte = 0; ibyte < nbytes; ibyte++)    /* for each byte in bytemap */
         bytevalues[(int) bytemap[ibyte]] = 1;  /*use its value to index bytevalues */
/* --- collect the 1's indexes in colors[] --- */
    for (igray = 0; igray < grayscale; igray++) /* check all possible values */
        if ((int) bytevalues[igray]) {  /*bytemap contains igray somewheres */
            colors[ncolors] = (intbyte) igray;  /* so store igray in colors */
            bytevalues[igray] = (intbyte) ncolors;      /* save colors[] index */
            if (maxcolors > 0 && ncolors >= maxcolors)  /* too many color indexes */
                bytevalues[igray] = (intbyte) (maxcolors - 1);  /*so scale back to max */
            ncolors++;
        }
    /* and bump #colors *//* --- rescale colors so largest, colors[ncolors-1], is black --- */
    if (isscale)
        /* only rescale if requested */
        if (ncolors > 1)        /* and if not a "blank" raster */
            if (colors[ncolors - 1] > 0) {      /*and at least one pixel non-white */
                /* --- multiply each colors[] by factor that scales largest to 255 --- */
                double scalefactor =
                    ((double) (grayscale - 1)) /
                    ((double) colors[ncolors - 1]);
                for (igray = 1; igray < ncolors; igray++) {     /* re-scale each colors[] */
                    colors[igray] =
                        min2(grayscale - 1,
                             (int) (scalefactor * colors[igray] + 0.5));
                    if (igray > 5)
                        colors[igray] =
                            min2(grayscale - 1, colors[igray] + 2 * igray);
                }
            }
    /* --- end-of-if(isscale) --- */
    /* --- apply gamma correction --- */
    if (isgamma                 /* only gamma correct if requested */
        && gammacorrection > 0.0001)    /* and if we have gamma correction */
        if (ncolors > 1)        /* and if not a "blank" raster */
            if (colors[ncolors - 1] > 0) {      /*and at least one pixel non-white */
                for (igray = 1; igray < ncolors; igray++) {     /*gamma correct each colors[] */
                    int grayval = colors[igray],        /* original 0=white to 255=black */
                        gmax = grayscale - 1;   /* should be 255 */
                    double dgray = ((double) (gmax - grayval)) / ((double) gmax);       /*0=black 1=white */
                    dgray = pow(dgray, (1.0 / gammacorrection));        /* apply gamma correction */
                    grayval = (int) (gmax * (1.0 - dgray) + 0.5);       /* convert back to grayval */
                    colors[igray] = grayval;
                }               /* store back in colors[] */
            }
    /* --- end-of-if(isgamma) --- */
    /* -------------------------------------------------------------------------
       Construct colormap
       -------------------------------------------------------------------------- */
    for (ibyte = 0; ibyte < nbytes; ibyte++)    /* for each byte in bytemap */
        colormap[ibyte] = bytevalues[(int) bytemap[ibyte]];     /*index for this value */
/* -------------------------------------------------------------------------
back to caller with #colors, or 0 for any error
-------------------------------------------------------------------------- */
  end_of_job:
    if (bytevalues != NULL)
        free(bytevalues);       /* free working memory */
    if (maxcolors > 0 && ncolors > maxcolors)   /* too many color indexes */
        ncolors = maxcolors;    /* return maximum to caller */
    return (ncolors);           /* back with #colors, or 0=error */
}                               /* --- end-of-function aacolormap() --- */
#endif

/* ==========================================================================
 * Function:    aaweights ( width, height )
 *      Builds "canonical" weight matrix, width x height, in a raster
 *      (see Notes below for discussion).
 * --------------------------------------------------------------------------
 * Arguments:   width (I)   int containing width (#cols) of returned
 *              raster/matrix of weights
 *      height (I)  int containing height (#rows) of returned
 *              raster/matrix of weights
 * --------------------------------------------------------------------------
 * Returns: ( raster * )    ptr to raster containing width x height
 *              weight matrix, or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o For example, given width=7, height=5, builds the matrix
 *          1 2 3  4 3 2 1
 *          2 4 6  8 6 4 2
 *          3 6 9 12 9 6 3
 *          2 4 6  8 6 4 2
 *          1 2 3  4 3 2 1
 *      If an even dimension given, the two center numbers stay
 *      the same, e.g., 123321 for the top row if width=6.
 *        o For an odd square n x n matrix, the sum of all n^2
 *      weights will be ((n+1)/2)^4.
 *        o The largest weight (in the allocated pixsz=8 raster) is 255,
 *      so the largest square matrix is 31 x 31.  Any weight that
 *      tries to grow beyond 255 is held constant at 255.
 *        o A new_raster(), pixsz=8, is allocated for the caller.
 *      To avoid memory leaks, be sure to delete_raster() when done.
 * ======================================================================= */
static raster *aaweights(int width, int height)
{
    raster *weights = NULL;
    int irow = 0;
    int icol = 0;
    int weight = 0;             /*running weight, as per Notes above */

    if ((weights = new_raster(width, height, 8)) == NULL) {
        goto end_of_job;
    }
/* -------------------------------------------------------------------------
Fill weight matrix, as per Notes above
-------------------------------------------------------------------------- */
    for (irow = 0; irow < height; irow++) {
        /* outer loop over rows */
        for (icol = 0; icol < width; icol++) {  /* inner loop over cols */
            int jrow = height - irow - 1,       /* backwards irow, height-1,...,0 */
                jcol = width - icol - 1;        /* backwards icol,  width-1,...,0 */
            weight = min2(irow + 1, jrow + 1) * min2(icol + 1, jcol + 1);       /* weight */
            if (aaalgorithm == 1)
                weight = 1;     /* force equal weights */
            setpixel(weights, irow, icol, min2(255, weight));   /*store weight in matrix */
        }
    }
  end_of_job:
    return weights;
}

/* ==========================================================================
 * Function:    aawtpixel ( image, ipixel, weights, rotate )
 * Purpose: Applies matrix of weights to the pixels
 *      surrounding ipixel in image, rotated clockwise
 *      by rotate degrees (typically 0 or 30).
 * --------------------------------------------------------------------------
 * Arguments:   image (I)   raster * to bitmap (though it can be bytemap)
 *              containing image with pixels to be averaged.
 *      ipixel (I)  int containing index (irow*width+icol) of
 *              center pixel of image for weighted average.
 *      weights (I) raster * to bytemap of relative weights
 *              (0-255), whose dimensions (usually odd width
 *              and odd height) determine the "subgrid" of
 *              image surrounding ipixel to be averaged.
 *      rotate (I)  int containing degrees clockwise rotation
 *              (typically 0 or 30), i.e., imagine weights
 *              rotated clockwise and then averaging the
 *              image pixels "underneath" it now.
 * --------------------------------------------------------------------------
 * Returns: ( int )     0-255 weighted average, or -1 for any error
 * --------------------------------------------------------------------------
 * Notes:     o The rotation matrix used (when requested) is
 *          / x' \     / cos(theta)  sin(theta)/a \  / x \
 *          |    |  =  |                          |  |   |
 *                  \ y' /     \ -a*sin(theta) cos(theta) /  \ y /
 *      where a=1 (current default) is the pixel (not screen)
 *      aspect ratio width:height, and theta is rotate (converted
 *      from degrees to radians).  Then x=col,y=row are the integer
 *      pixel coords relative to the input center ipixel, and
 *      x',y' are rotated coords which aren't necessarily integer.
 *      The actual pixel used is the one nearest x',y'.
 * ======================================================================= */
int aawtpixel(raster * image, int ipixel, raster * weights, int rotate)
{
    int aaimgval = 0,           /* weighted avg returned to caller */
        totwts = 0, sumwts = 0; /* total of all wts, sum wts used */
    int pixsz = image->pixsz,   /* #bits per image pixel */
        black1 = 1, black8 = 255,       /* black for 1-bit, 8-bit pixels */
        black = (pixsz == 1 ? black1 : black8), /* black value for our image */
        scalefactor = (black1 + black8 - black),        /* only scale 1-bit images */
        iscenter = 0;           /* set true if center pixel black */
/* --- grid dimensions and indexes --- */
    int wtheight = weights->height,     /* #rows in weight matrix */
        wtwidth = weights->width,       /* #cols in weight matrix */
        imgheight = image->height,      /* #rows in image */
        imgwidth = image->width;        /* #cols in image */
    int wtrow, wtrow0 = wtheight / 2,   /* center row index for weights */
        wtcol, wtcol0 = wtwidth / 2,    /* center col index for weights */
        imgrow, imgrow0 = ipixel / imgwidth,    /* center row index for ipixel */
        imgcol, imgcol0 = ipixel - (imgrow0 * imgwidth);        /*center col for ipixel */
/* --- rotated grid variables --- */
    static int prevrotate = 0;  /* rotate from previous call */
    static double costheta = 1.0,       /* cosine for previous rotate */
        sintheta = 0.0;         /* and sine for previous rotate */
    double a = 1.0;             /* default aspect ratio */
/* -------------------------------------------------------------------------
Initialization
-------------------------------------------------------------------------- */
/* --- refresh trig functions for rotate when it changes --- */
    if (rotate != prevrotate) { /* need new sine/cosine */
        costheta = cos(((double) rotate) / 57.29578);   /*cos of rotate in radians */
        sintheta = sin(((double) rotate) / 57.29578);   /*sin of rotate in radians */
        prevrotate = rotate;
    }
    /* save current rotate as prev *//* -------------------------------------------------------------------------
       Calculate aapixel as weighted average over image points around ipixel
       -------------------------------------------------------------------------- */
        for (wtrow = 0; wtrow < wtheight; wtrow++) {
        for (wtcol = 0; wtcol < wtwidth; wtcol++) {
            /* --- allocations and declarations --- */
            int wt = (int) getpixel(weights, wtrow, wtcol);     /* weight for irow,icol */
            int drow = wtrow - wtrow0,  /* delta row offset from center */
                dcol = wtcol - wtcol0;  /* delta col offset from center */
            /* not used --int iscenter = 0; *//* set true if center point black */
            /* --- initialization --- */
            totwts += wt;       /* sum all weights */
            /* --- rotate (if requested) --- */
            if (rotate != 0) {  /* non-zero rotation */
                /* --- apply rotation matrix to (x=dcol,y=drow) --- */
                double dx = (double) dcol, dy = (double) drow, dtemp;   /* need floats */
                dtemp = dx * costheta + dy * sintheta / a;      /* save new dx' */
                dy = -a * dx * sintheta + dy * costheta;        /* dy becomes new dy' */
                dx = dtemp;     /* just for notational convenience */
                /* --- replace original (drow,dcol) with nearest rotated point --- */
                drow = (int) (dy + 0.5);        /* round dy for nearest row */
                dcol = (int) (dx + 0.5);        /* round dx for nearest col */
            }
            /* --- end-of-if(rotate!=0) --- */
            /* --- select image pixel to be weighted --- */
            imgrow = imgrow0 + drow;    /*apply displacement to center row */
            imgcol = imgcol0 + dcol;    /*apply displacement to center col */
            /* --- if pixel in bounds, accumulate weighted average --- */
            if (imgrow >= 0 && imgrow < imgheight) {    /* row is in bounds */
                if (imgcol >= 0 && imgcol < imgwidth) { /* and col is in bounds */
                    /* --- accumulate weighted average --- */
                    int imgval = (int) getpixel(image, imgrow, imgcol); /* image value */
                    aaimgval += wt * imgval;    /* weighted sum of image values */
                    sumwts += wt;       /* and also sum weights used */
                    /* --- check if center image pixel black --- */
                    if (drow == 0 && dcol == 0) {       /* this is center ipixel */
                        if (imgval == black) {  /* and it's black */
                            /* not used --iscenter = 1; *//* so set black center flag true */
                        }
                    }
                }
            }
        }
    }
    if (0 && iscenter) {         /* center point is black */
        aaimgval = black8;
    } else {                        /* center point not black */
        aaimgval = ((totwts / 2 - 1) + scalefactor * aaimgval) / totwts;
    }
    return aaimgval;
}

/* ==========================================================================
 * Function:    mimetexsetmsg ( newmsglevel, newmsgfp )
 * Purpose: Sets msglevel and msgfp, usually called from
 *      an external driver (i.e., DRIVER not defined
 *      in this module).
 * --------------------------------------------------------------------------
 * Arguments:   newmsglevel (I) int containing new msglevel
 *              (unchanged if newmsglevel<0)
 *      newmsgfp (I)    FILE * containing new msgfp
 *              (unchanged if newmsgfp=NULL)
 * --------------------------------------------------------------------------
 * Returns: ( int )     always 1
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
int mimetexsetmsg(int newmsglevel, FILE * newmsgfp)
{
    if (newmsglevel >= 0)
        msglevel = newmsglevel;
    if (newmsgfp != NULL)
        msgfp = newmsgfp;
    return 1;
}

