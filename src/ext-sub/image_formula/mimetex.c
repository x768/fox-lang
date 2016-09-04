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
 *      delete_subraster(sp)  deallocate subraster_t (sp=subraster_t ptr)
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
 *      get_charsubraster(symdef,size)  wrap subraster_t around chardef
 *      get_symsubraster(symbol,size)    returns subraster_t for symbol
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

#include "smalltex_pri.h"
#include "mimetex.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>



/* -------------------------------------------------------------------------
control flags and values
-------------------------------------------------------------------------- */

int daemonlevel = 0;     /* incremented in main() */
int recurlevel = 0;      /* inc/decremented in rasterize() */
int scriptlevel = 0;     /* inc/decremented in rastlimits() */
int isligature = 0;      /* true if ligature found */
const char *subexprptr = NULL;   /* ptr within expression to subexpr */
int fraccenterline = NOVALUE;   /* baseline for punct. after \frac */

/*SHARED(int,imageformat,1); *//* image is 1=bitmap, 2=.gf-like */
int isdisplaystyle = 1; /* displaystyle mode (forced if 2) */
int ispreambledollars = 0;      /* displaystyle mode set by $$...$$ */
int fontnum = 0;        /* cal=1,scr=2,rm=3,it=4,bb=5,bf=6 */
int fontsize = NORMALSIZE;      /* current size */
int magstep = 1;        /* magstep (1=no change) */
int displaysize = DISPLAYSIZE;  /* use \displaystyle when fontsize>= */
int shrinkfactor = 3;   /* shrinkfactors[fontsize] */
int rastlift = 0;       /* rastraise() lift parameter */
int rastlift1 = 0;      /* rastraise() lift for base exprssn */
double unitlength = 1.0;        /* #pixels per unit (may be <1.0) */
int iunitlength = 1;    /* #pixels per unit as int for store */
/*GLOBAL(int,textwidth,TEXTWIDTH); *//* #pixels across line */
//GLOBAL(int, adfrequency, ADFREQUENCY);    /* advertisement frequency */
int isnocatspace = 0;   /* >0 to not add space in rastcat() */
int smashmargin = SMASHMARGIN;  /* minimum "smash" margin */
int mathsmashmargin = SMASHMARGIN;      /* needed for \text{if $n-m$ even} */
int issmashdelta = 1;   /* true if smashmargin is a delta */
int isexplicitsmash = 0;        /* true if \smash explicitly given */
int smashcheck = SMASHCHECK;    /* check if terms safe to smash */
//GLOBAL(int, isnomath, 0);     /* true to inhibit math mode */
int isscripted = 0;     /* is (lefthand) term text-scripted */
int isdelimscript = 0;  /* is \right delim text-scripted */
int issmashokay = 0;    /*is leading char okay for smashing */
int blanksignal = BLANKSIGNAL;  /*rastsmash signal right-hand blank */
int blanksymspace = 0;  /* extra (or too much) space wanted */
int aaalgorithm = AAALGORITHM;  /* for lp, 1=aalowpass, 2 =aapnm */
int maxfollow = MAXFOLLOW;      /* aafollowline() maxturn parameter */
int fgalias = 1;
int fgonly = 0;
int bgalias = 0;
int bgonly = 0;         /* aapnm() params */
int issupersampling = ISSUPERSAMPLING;  /*1=supersampling 0=lowpass */
int isss = ISSUPERSAMPLING;     /* supersampling flag for main() */
int *workingparam = NULL;       /* working parameter */
subraster_t *workingbox = NULL;   /*working subraster_t box */
int isreplaceleft = 0;  /* true to replace leftexpression */
subraster_t *leftexpression = NULL;       /*rasterized so far */
const mathchardef_t *leftsymdef = NULL; /* mathchardef for preceding symbol */
/*GLOBAL(int,currentcharclass,NOVALUE); *//*primarily to check for PUNCTION */

/* --- variables for anti-aliasing parameters --- */
int centerwt = CENTERWT;        /*lowpass matrix center pixel wt */
int adjacentwt = ADJACENTWT;    /*lowpass matrix adjacent pixel wt */
int cornerwt = CORNERWT;        /*lowpass matrix corner pixel wt */
int minadjacent = MINADJACENT;  /* darken if>=adjacent pts black */
int maxadjacent = MAXADJACENT;  /* darken if<=adjacent pts black */
int weightnum = 1;              /* font wt, */
int maxaaparams = 4;            /* #entries in table */

int nfontinfo = 11;             /* legal font#'s are 1...nfontinfo */
fontfamily_t *fonttable = (ISSUPERSAMPLING ? ssfonttable : aafonttable);

/* -------------------------------------------------------------------------
control flags and values
-------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
store for evalterm() [n.b., these are stripped-down funcs from nutshell]
-------------------------------------------------------------------------- */

const store_t mimestore[MAXSTORE] = {
    {"fontsize", &fontsize}, {"fs", &fontsize}, /* font size */
    {"fontnum", &fontnum}, {"fn", &fontnum},    /* font number */
    {"unitlength", &iunitlength},       /* unitlength */
    /*{ "mytestvar", &mytestvar }, */
    {NULL, NULL}
};

/* --- supersampling shrink factors corresponding to displayed sizes --- */
const int shrinkfactors[] =    /*supersampling shrinkfactor by size */
{ 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

/* ---
 * space between adjacent symbols, e.g., symspace[RELATION][VARIABLE]
 * ------------------------------------------------------------------ */
const int symspace[11][11] = {
/* -----------------------------------------------------------------------
 * Right... ORD OPER  BIN  REL OPEN CLOS PUNC  VAR DISP SPACE unused
 * Left...
 * --------------------------------------------------------------------- */

     /*ORDINARY*/ {2, 3, 3, 5, 3, 2, 2, 2, 3, 0, 0},
     /*OPERATOR*/ {3, 1, 1, 5, 3, 2, 2, 2, 3, 0, 0},
     /*BINARYOP*/ {2, 1, 1, 5, 3, 2, 2, 2, 3, 0, 0},
     /*RELATION*/ {5, 5, 5, 2, 5, 5, 2, 5, 5, 0, 0},
     /*OPENING*/  {2, 2, 2, 5, 2, 4, 2, 2, 3, 0, 0},
     /*CLOSING*/  {2, 3, 3, 5, 4, 2, 1, 2, 3, 0, 0},
     /*PUNCTION*/ {2, 2, 2, 5, 2, 2, 1, 2, 2, 0, 0},
     /*VARIABLE*/ {2, 2, 2, 5, 2, 2, 1, 2, 2, 0, 0},
     /*DISPOPER*/ {2, 3, 3, 5, 2, 3, 2, 2, 2, 0, 0},
     /*SPACEOPER*/{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    /*unused */   {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};


/* ---
 * font family information
 * ----------------------- */
/* note: class(1=upper,2=alpha,3=alnum,4=lower,5=digit,9=all) now unused */
const fontinfo_t fontinfo[] = {
    { "\\math", 0, 0, 0},         /*(0) default math mode */
    { "\\mathcal", CMSY10, 0, 1}, /*(1) calligraphic, uppercase */
    { "\\mathscr", RSFS10, 0, 1}, /*(2) rsfs/script, uppercase */
    { "\\textrm", CMR10, 1, -1},  /*(3) \rm,\text{abc} --> {\textrm~abc} */
    { "\\textit", CMMI10, 1, -1}, /*(4) \it,\textit{abc}-->{\textit~abc} */
    { "\\mathbb", BBOLD10, 0, -1},        /*(5) \bb,\mathbb{abc}-->{\mathbb~abc} */
    { "\\mathbf", CMMIB10, 0, -1},        /*(6) \bf,\mathbf{abc}-->{\mathbf~abc} */
    { "\\mathrm", CMR10, 0, -1},  /*(7) \mathrm */
    { "\\cyr", CYR10, 1, -1},     /*(8) \cyr (defaults as text mode) */
    { "\\textgreek", CMMI10GR, 1, -1},    /*(9) \textgreek{ab}-->\alpha\beta */
    { "\\textbfgreek", CMMI10BGR, 1, -1}, /*(10)\textbfgreek{ab}-->\alpha\beta */
    { "\\textbbgreek", BBOLD10GR, 1, -1}, /*(11)\textbbgreek{ab}-->\alpha\beta */
    { NULL, 0, 0, 0}
};



/* ============================================================================= */

static char *findbraces(const char *expression, const char *command);
static int get_baseline(const chardef_t * gfdata);
static int get_ligature(const char *expression, int family);
static int getstore(const store_t * store, const char *identifier);
static raster_t *gftobitmap(const raster_t * gf);
static int isbrace(const char *expression, const char *braces, int isescape);
static int line_recurse(raster_t * rp, double row0, double col0, double row1,
                        double col1, int thickness);
subraster_t *make_delim(const char *symbol, int height);
static subraster_t *rastdispmath(const char **expression, int size,
                               subraster_t * sp);
static int rastsmash(subraster_t * sp1, subraster_t * sp2);

/* ============================================================================= */

/* ============================================================================
 * Allocation and constructor for raster.
 * 
 * @width: containing width, in bits.
 * @height: containing height, in bits/scans.
 * @pixsz: containing #bits per pixel, 1 or 8
 * 
 * @returns: ptr to allocated and initialized, or NULL for any error.
 * ===========================================================================*/
raster_t *new_raster(int width, int height, int pixsz)
{
    raster_t *rp = NULL;
    pixbyte_t *pixmap = NULL;
    int nbytes = pixsz * bitmapsz(width, height);       /* #bytes needed for pixmap */
    int npadding = (0 && issupersampling ? 8 + 256 : 0);        /* padding bytes */

#if DEBUG_LOG_LEVEL >= 9999
    fprintf(msgfp, "new_raster(%d,%d,%d)> entry point\n",
            width, height, pixsz);
#endif

    rp = malloc(sizeof(raster_t));

    if (rp == NULL) {
        goto end_of_job;
    }

    rp->width = width;
    rp->height = height;
    rp->format = 1;             /* initialize as bitmap format */
    rp->pixsz = pixsz;          /* store #bits per pixel */
    rp->pixmap = NULL;

    if (nbytes > 0 && nbytes <= pixsz * maxraster) {    /* fail if width*height too big */
        pixmap = malloc(nbytes + npadding);
    }

    if (pixmap == NULL) {
        delete_raster(rp);
        rp = NULL;
        goto end_of_job;
    }
    memset(pixmap, 0, nbytes);    /* init bytes to binary 0's */
    *pixmap = 0;                /* and first byte always 0 */
    rp->pixmap = pixmap;

end_of_job:
#if DEBUG_LOG_LEVEL >= 9999
    fprintf(msgfp, "new_raster(%d,%d,%d)> returning (%s)\n",
            width, height, pixsz,
            (rp == NULL ? "null ptr" : "success"));
#endif

    return rp;
}


/* ============================================================================
 * Allocate a new subraster_t along with an embedded raster of width x height.
 * if width or height <=0, embedded raster not allocated.
 * 
 * @width: containing width, in bits.
 * @height: containing height, in bits/scans.
 * @pixsz: containing #bits per pixel, 1 or 8
 * 
 * @returns: ptr to newly-allocated subraster_t, or NULL for any error.
 * ===========================================================================*/
subraster_t *new_subraster(int width, int height, int pixsz)
{
    subraster_t *sp = NULL;       /* subraster_t returned to caller */
    raster_t *rp = NULL;          /* image raster embedded in sp */
    int size = NORMALSIZE;      /* default size */
    int baseline = height - 1;  /* and baseline */

#if DEBUG_LOG_LEVEL >= 9999
    fprintf(msgfp, "new_subraster(%d,%d,%d)> entry point\n",
            width, height, pixsz);
#endif

    sp = malloc(sizeof(subraster_t));
    if (sp == NULL)
        goto end_of_job;

    sp->type = NOVALUE;         /* character or image raster */
    sp->symdef = NULL;          /* mathchardef identifying image */
    sp->baseline = baseline;    /*0 if image is entirely descending */
    sp->size = size;            /* font size 0-4 */
    sp->toprow = sp->leftcol = (-1);    /* upper-left corner of subraster_t */
    sp->image = NULL;           /*ptr to bitmap image of subraster_t */

    /* --- allocate raster struct if desired --- */
    if (width > 0 && height > 0 && pixsz > 0) {
        rp = new_raster(width, height, pixsz);
        if (rp != NULL) {
            sp->image = rp;
        } else {
            delete_subraster(sp);
            sp = NULL;
        }
    }

end_of_job:
#if DEBUG_LOG_LEVEL >= 9999
    fprintf(msgfp, "new_subraster(%d,%d,%d)> returning (%s)\n",
            width, height, pixsz,
            (sp == NULL ? "null ptr" : "success"));
#endif

    return sp;
}

/* ============================================================================
 * Allocates and initializes a chardef struct,
 * but _not_ the embedded raster struct.
 * 
 * @returns: ptr to allocated and initialized, or NULL for any error.
 * ===========================================================================*/
chardef_t *new_chardef(void)
{
    chardef_t *cp = malloc(sizeof(chardef_t));

    if (cp == NULL)
        goto end_of_job;

    cp->charnum = cp->location = 0;     /* init character description */
    cp->toprow = cp->topleftcol = 0;    /* init upper-left corner */
    cp->botrow = cp->botleftcol = 0;    /* init lower-left corner */
    cp->image.width = cp->image.height = 0;     /* init raster dimensions */
    cp->image.format = 0;       /* init raster format */
    cp->image.pixsz = 0;        /* and #bits per pixel */
    cp->image.pixmap = NULL;    /* init raster pixmap as null */

end_of_job:
    return cp;
}

/* ============================================================================
 * Frees memory for raster bitmap and struct.
 * 
 * @rp: ptr to raster struct to be deleted.
 * ===========================================================================*/
void delete_raster(raster_t *rp)
{
    if (rp != NULL) {
        free(rp->pixmap);   /* ignored if (rp->pixmap == NULL) */
        free(rp);
    }
}

/* ============================================================================
 * Deallocates a subraster_t (and embedded raster).
 * 
 * @rp: ptr to subraster_t struct to be deleted.
 * ===========================================================================*/
void delete_subraster(subraster_t *sp)
{
    if (sp != NULL) {
        if (sp->type != CHARASTER) {    /* not static character data */
            delete_raster(sp->image);   /* ignored if (sp->image == NULL) */
        }
        free(sp);
    }
}

/* ============================================================================
 * Deallocates a chardef (and bitmap of embedded raster).
 * 
 * @rp: ptr to chardef struct to be deleted.
 * ===========================================================================*/
void delete_chardef(chardef_t *cp)
{
    if (cp != NULL) {
        free(cp->image.pixmap);     /* ignored if (cp->image.pixmap == NULL) */
        free(cp);
    }
}

/* ============================================================================
 * makes duplicate copy of rp.
 * 
 * @rp: ptr to raster struct to be copied.
 * 
 * @returns: ptr to new copy rp, or NULL for any error.
 * ===========================================================================*/
raster_t *rastcpy(const raster_t *rp)
{
    raster_t *newrp = NULL;

    if (rp != NULL) {
        int height = rp->height;
        int width = rp->width;
        int pixsz = rp->pixsz;
        int nbytes = pixmapsz(rp);

        newrp = new_raster(width, height, pixsz);
        if (newrp != NULL) {
            memcpy(newrp->pixmap, rp->pixmap, nbytes);
        }
    }

    return newrp;
}

/* ============================================================================
 * makes duplicate copy of rp.
 * 
 * @rp: ptr to subraster_t struct to be copied.
 * 
 * @returns: ptr to new copy rp, or NULL for any error.
 * ===========================================================================*/
subraster_t *subrastcpy(const subraster_t *sp)
{
    subraster_t *newsp = NULL;    /* allocate new subraster_t */
    raster_t *newrp = NULL;       /* and new raster image within it */

    if (sp == NULL) {
        goto end_of_job;
    }
/* --- allocate new subraster_t "envelope" for copy --- */
    newsp = new_subraster(0, 0, 0);
    if (newsp == NULL) {
        goto end_of_job;
    }
/* --- transparently copy original envelope to new one --- */
    memcpy(newsp, sp, sizeof(subraster_t));

    if (sp->image != NULL) {
        newrp = rastcpy(sp->image);
        if (newrp == NULL) {
            delete_subraster(newsp);
            newsp = NULL;
            goto end_of_job;
        }
    }
    /* back to caller with error signal */
    /* --- set new params in new envelope --- */
    newsp->image = newrp;
    switch (sp->type) {
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

end_of_job:
    return newsp;
}

/* ============================================================================
 * rotates rp image 90 degrees right/clockwise
 * Notes: An underbrace is } rotated 90 degrees clockwise,
 *        a hat is <, etc.
 * 
 * @rp: ptr to raster struct to be rotated
 * 
 * @returns: ptr to new raster rotated relative to rp, or NULL for any error.
 * ===========================================================================*/
raster_t *rastrot(const raster_t *rp)
{
    raster_t *rotated = NULL;             /*rotated raster returned to caller */
    int height = rp->height, irow;      /* original height, row index */
    int width = rp->width, icol;        /* original width, column index */
    int pixsz = rp->pixsz;              /* #bits per pixel */

    /* --- allocate rotated raster with flipped width<-->height --- */
    rotated = new_raster(height, width, pixsz);
    if (rotated  != NULL) {
        /* --- fill rotated raster --- */
        for (irow = 0; irow < height; irow++) { /* for each row of rp */
            for (icol = 0; icol < width; icol++) {      /* and each column of rp */
                int value = getpixel(rp, irow, icol);
                /* setpixel(rotated,icol,irow,value); */
                setpixel(rotated, icol, (height - 1 - irow), value);
            }
        }
    }
    return rotated;
}

/* ============================================================================
 * magnifies rp by integer magstep,
 * e.g., double-height and double-width if magstep=2
 * 
 * @rp: ptr to raster struct to be magnified
 * @a_magstep: magnification scale, e.g., 2 to double the width and height of rp
 * 
 * @returns: ptr to new raster magnified relative to rp, or NULL for any error.
 * ===========================================================================*/
raster_t *rastmag(const raster_t *rp, int a_magstep)
{
    raster_t *magnified = NULL;
    int height = rp->height, irow,      /* height, row index */
        width = rp->width, icol,        /* width, column index */
        mrow = 0, mcol = 0,     /* dup pixels magstep*magstep times */
        pixsz = rp->pixsz;      /* #bits per pixel */

    if (rp == NULL) {
        goto end_of_job;
    }
    if (a_magstep < 1 || a_magstep > 10) {
        goto end_of_job;
    }

    magnified = new_raster(a_magstep * width, a_magstep * height, pixsz);
    if (magnified == NULL)
        goto end_of_job;

    /* --- fill reflected raster --- */
    for (irow = 0; irow < height; irow++) { /* for each row of rp */
        for (mrow = 0; mrow < a_magstep; mrow++) {  /* dup row magstep times */
            for (icol = 0; icol < width; icol++) {  /* and for each column of rp */
                for (mcol = 0; mcol < a_magstep; mcol++) {  /* dup col magstep times */
                    int value = getpixel(rp, irow, icol);
                    int row1 = irow * a_magstep;
                    int col1 = icol * a_magstep;
                    setpixel(magnified, (row1 + mrow), (col1 + mcol), value);
                }
            }
        }
    }

end_of_job:
    return magnified;
}

/* ============================================================================
 * magnifies a bytemap by magstep,
 * e.g., double-height and double-width if magstep=2
 * Notes:     o Apply EPX/Scale2x/AdvMAME2x  for magstep 2,
 *      and Scale3x/AdvMAME3x  for magstep 3,
 *      as described by http://en.wikipedia.org/wiki/2xSaI
 * 
 * @bytemap: pixbyte_t * ptr to byte map to be magnified
 * @width: cols in original bytemap
 * @height: rows in original bytemap
 * @magstep: magnification scale
 * 
 * @returns: ptr to new bytemap magnified, or NULL for any error.
 * ===========================================================================*/
pixbyte_t *bytemapmag(const pixbyte_t *bytemap, int width, int height, int a_magstep)
{
    pixbyte_t *magnified = NULL;  /* magnified bytemap back to caller */
    int irow, icol;             /* original height, width indexes */
    int mrow = 0, mcol = 0;     /* dup bytes magstep*magstep times */
    int imap = -1;              /* original bytemap[] index */
    int byteval = 0;            /* byteval=bytemap[imap] */
    int isAdvMAME = 1;          /* true to apply AdvMAME2x and 3x */
    int icell[10];              /* bytemap[] nearest neighbors */
    int bmmdiff = 64;           /* nearest neighbor diff allowed */
#define bmmeq(i,j) ((absval((icell[i]-icell[j]))<=bmmdiff))     /*approx equal */

    if (bytemap == NULL)
        goto end_of_job;
    if (width < 1 || height < 1)
        goto end_of_job;
    if (width * height > 100000)
        goto end_of_job;
    if (a_magstep < 1 || a_magstep > 10)
        goto end_of_job;
    
    magnified = malloc(a_magstep * width * a_magstep * height);
    if (magnified == NULL)
        goto end_of_job;

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
                    magnified[imag] = (pixbyte_t) (byteval);
                    /* --- apply AdvMAME2x and 3x (if desired) --- */
                    if (isAdvMAME) {        /* AdvMAME2x and 3x wanted */
                        int mcell = 1 + mcol + a_magstep * mrow;    /*1,2,3,4 or 1,2,3,4,5,6,7,8,9 */

                        icell[5] = byteval; /* center cell of 3x3 bytemap[] */
                        icell[4] = (icol > 0 ? (int) (bytemap[imap - 1]) : byteval);        /*left of center */
                        icell[6] = (icol < width ? (int) (bytemap[imap + 1]) : byteval);    /*right */
                        icell[2] = (irow > 0 ? (int) (bytemap[imap - width]) : byteval);    /*above center */
                        icell[8] = (irow < height ? (int) (bytemap[imap + width]) : byteval);       /*below */
                        icell[1] = (irow > 0 && icol > 0 ? (int) (bytemap[imap - width - 1]) : byteval);
                        icell[3] = (irow > 0 && icol < width ? (int) (bytemap[imap - width + 1]) : byteval);
                        icell[7] = (irow < height && icol > 0 ? (int) (bytemap[imap + width - 1]) : byteval);
                        icell[9] = (irow < height && icol < width ? (int) (bytemap[imap + width + 1]) : byteval);

                        switch (a_magstep) {        /* 2x magstep=2, 3x magstep=3 */
                        default:
                            break;  /* no AdvMAME at other magsteps */
                        case 2:    /* AdvMAME2x */
                            if (mcell == 1)
                                if (bmmeq(4, 2) && !bmmeq(4, 8) && !bmmeq(2, 6))
                                    magnified[imag] = icell[2];
                            if (mcell == 2)
                                if (bmmeq(2, 6) && !bmmeq(2, 4) && !bmmeq(6, 8))
                                    magnified[imag] = icell[6];
                            if (mcell == 4)
                                if (bmmeq(6, 8) && !bmmeq(6, 2) && !bmmeq(8, 4))
                                    magnified[imag] = icell[8];
                            if (mcell == 3)
                                if (bmmeq(8, 4) && !bmmeq(8, 6) && !bmmeq(4, 2))
                                    magnified[imag] = icell[4];
                            break;
                        case 3:    /* AdvMAME3x */
                            if (mcell == 1)
                                if (bmmeq(4, 2) && !bmmeq(4, 8) && !bmmeq(2, 6))
                                    magnified[imag] = icell[4];
                            if (mcell == 2)
                                if ((bmmeq(4, 2) && !bmmeq(4, 8) && !bmmeq(2, 6) && !bmmeq(5, 3)) || (bmmeq(2, 6) && !bmmeq(2, 4) && !bmmeq(6, 8) && !bmmeq(5, 1)))
                                    magnified[imag] = icell[2];
                            if (mcell == 3)
                                if (bmmeq(2, 6) && !bmmeq(2, 4) && !bmmeq(6, 8))
                                    magnified[imag] = icell[6];
                            if (mcell == 4)
                                if ((bmmeq(8, 4) && !bmmeq(8, 6) && !bmmeq(4, 2) && !bmmeq(5, 1)) || (bmmeq(4, 2) && !bmmeq(4, 8) && !bmmeq(2, 6) && !bmmeq(5, 7)))
                                    magnified[imag] = icell[4];
                            if (mcell == 6)
                                if ((bmmeq(2, 6) && !bmmeq(2, 4) && !bmmeq(6, 8) && !bmmeq(5, 9)) || (bmmeq(6, 8) && !bmmeq(6, 2) && !bmmeq(8, 4) && !bmmeq(5, 3)))
                                    magnified[imag] = icell[6];
                            if (mcell == 7)
                                if (bmmeq(8, 4) && !bmmeq(8, 6) && !bmmeq(4, 2))
                                    magnified[imag] = icell[4];
                            if (mcell == 8)
                                if ((bmmeq(6, 8) && !bmmeq(6, 2) && !bmmeq(8, 4) && !bmmeq(5, 7)) || (bmmeq(8, 4) && !bmmeq(8, 6) && !bmmeq(4, 2) && !bmmeq(5, 9)))
                                    magnified[imag] = icell[8];
                            if (mcell == 9)
                                if (bmmeq(6, 8) && !bmmeq(6, 2) && !bmmeq(8, 4))
                                    magnified[imag] = icell[6];
                            break;
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

/* ============================================================================
 * reflects rp.
 * 
 * @rp: ptr to raster struct to be reflected
 * @axis: 1 for horizontal reflection, or 2 for vertical
 * 
 * @returns: ptr to new raster reflected, or NULL for any error.
 * ===========================================================================*/
raster_t *rastref(const raster_t * rp, int axis)
{
    raster_t *reflected = NULL;   /* reflected raster back to caller */
    int height = rp->height, irow,      /* height, row index */
        width = rp->width, icol,        /* width, column index */
        pixsz = rp->pixsz;      /* #bits per pixel */

    reflected = new_raster(width, height, pixsz);
    if (reflected != NULL) {
        /* --- fill reflected raster --- */
        for (irow = 0; irow < height; irow++) {     /* for each row of rp */
            for (icol = 0; icol < width; icol++) {  /* and each column of rp */
                int value = getpixel(rp, irow, icol);
                if (axis == 1) {
                    setpixel(reflected, irow, width - 1 - icol, value);
                }
                if (axis == 2) {
                    setpixel(reflected, height - 1 - irow, icol, value);
                }
            }
        }
    }

    return reflected;
}

/* ============================================================================
 * Overlays source onto target,
 * with the 0,0-bit of source onto the top,left-bit of target.
 * 
 * @target: ptr to target raster struct
 * @source: ptr to source raster struct
 * @top: 0 ... target->height - 1
 * @lert: 0 ... target->width - 1
 * @isopaque: false (zero) to allow original 1-bits of target to
 *            "show through" 0-bits of source.
 * 
 * @returns: 1 if completed successfully, or 0 for any error.
 * ===========================================================================*/
int rastput(raster_t *target, const raster_t *source, int top, int left, int isopaque)
{
    int irow, icol,             /* indexes over source raster */
     twidth = target->width, theight = target->height,  /*target width,height */
        tpix, ntpix = twidth * theight; /* #pixels in target */
    int isfatal = 0,            /* true to abend on out-of-bounds error */
        isokay = 1;             /* true if no pixels out-of-bounds */

/* -------------------------------------------------------------------------
superimpose source onto target, one bit at a time
-------------------------------------------------------------------------- */
    if (top < 0 || left < 0) {
        isokay = 0;
        goto end_of_job;
    }

    for (irow = 0; irow < source->height; irow++) { /* for each scan line */
        tpix = (top + irow) * target->width + left - 1;     /*first target pixel (-1) */
        for (icol = 0; icol < source->width; icol++) {      /* each pixel in scan line */
            int svalue = getpixel(source, irow, icol);      /* source pixel value */
            ++tpix;         /* bump target pixel */

#if DEBUG_LOG_LEVEL >= 9999
            fprintf(msgfp,
                    "rastput> tpix,ntpix=%d,%d top,irow,theight=%d,%d,%d "
                    "left,icol,twidth=%d,%d,%d\n", tpix, ntpix,
                    top, irow, theight, left, icol, twidth);
#endif

            if (tpix >= ntpix || (irow + top >= theight || icol + left >= twidth)) {
                /* bounds check failed */
                isokay = 0; /* reset okay flag */
                if (isfatal) {
                    goto end_of_job;        /* abort if error is fatal */
                } else {
                    break;
                }
            }
            if (tpix >= 0) {        /* bounds check okay */
                if (svalue != 0 || isopaque) {      /*got dark or opaque source */
                    setpixel(target, irow + top, icol + left, svalue);
                }           /*overlay source on targ */
            }
        }
    }
end_of_job:
    return isokay;
}

/* ============================================================================
 * Overlays sp2 on top of sp1, leaving both unchanged
 *      and returning a newly-allocated composite subraster_t.
 *      Frees/deletes input sp1 and/or sp2 depending on value
 *      of isfree (0=none, 1=sp1, 2=sp2, 3=both).
 * --------------------------------------------------------------------------
 * @sp1: "underneath" subraster_t, whose baseline is preserved
 * @sp2: "overlaid" subraster_t
 * @offset2: 0 or number of pixels to horizontally shift sp2 relative to sp1,
 *           either positive (right) or negative
 * @isalign: 1 to align baselines, or 0 to vertically center sp2 over sp1.
 *           For isalign=2, images are vertically centered,
 *           but then adjusted by \raisebox lifts,
 *           using global variables rastlift1 for sp1 and rastlift for sp2.
 * @isfree:  1=free sp1 before return, 2=free sp2, 3=free both, 0=free none.
 * 
 * @returns: pointer to constructed subraster_t or  NULL for any error
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
 *            base       overlay
 *       --- +------------------------+ ---   For the overlay to be
 *        ^  |   ^        +----------+|  ^    vertically centered with
 *        |  |   |        |          ||  |    respect to the base,
 *        |  |   |B-b1    |          ||  |      B - b1 = H-B -(h1-b1), so
 *        |  |   v        |          ||  |      2*B = H-h1 + 2*b1
 *        |  |+----------+|          ||  |      B = b1 + (H-h1)/2
 *        B  ||  ^    ^  ||          ||  |    And when the base image is
 *        |  ||  |    |  ||          ||  |    bigger, H=h1 and B=b1 is
 *        |  ||  b1   |  ||          ||  |    the obvious correct answer.
 *        |  ||  |    h1 ||          || H=h2
 *        v  ||  v    |  ||          ||  |
 * ----------||-------|--||          ||--|--------
 * baseline  || h1-b1 v  || overlay  ||  |
 * for base  |+----------+| baseline ||  |
 * and com-  |   ^        | ignored  ||  |
 * posite    |   |H-B-    |----------||  |
 *           |   | (h1-b1)|          ||  |
 *           |   v        +----------+|  v
 *           +------------------------+ ---
 * ======================================================================= */
subraster_t *rastcompose(subraster_t *sp1, subraster_t *sp2, int offset2, int isalign, int isfree)
{
    subraster_t *sp = NULL;       /* returned subraster_t */
    raster_t *rp = NULL;          /* new composite raster in sp */
    int base1 = sp1->baseline,  /*baseline for underlying subraster_t */
        height1 = (sp1->image)->height, /* height for underlying subraster_t */
        width1 = (sp1->image)->width,   /* width for underlying subraster_t */
        pixsz1 = (sp1->image)->pixsz,   /* pixsz for underlying subraster_t */
        base2 = sp2->baseline,  /*baseline for overlaid subraster_t */
        height2 = (sp2->image)->height, /* height for overlaid subraster_t */
        width2 = (sp2->image)->width,   /* width for overlaid subraster_t */
        pixsz2 = (sp2->image)->pixsz;   /* pixsz for overlaid subraster_t */
    int height = max2(height1, height2),        /*composite height if sp2 centered */
        base = base1 + (height - height1) / 2,  /* and composite baseline */
        tlc2 = (height - height2) / 2,  /* top-left corner for overlay */
        width = 0, pixsz = 0;   /* other params for composite */
    int lift1 = rastlift1,      /* vertical \raisebox lift for sp1 */
        lift2 = rastlift;       /* vertical \raisebox lift for sp2 */


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

/* --- allocate returned subraster_t (and then initialize it) --- */
    sp = new_subraster(width, height, pixsz);
    if (sp == NULL)
        goto end_of_job;

    sp->type = IMAGERASTER;     /* image */
    sp->baseline = base;        /* composite baseline */
    sp->size = sp1->size;       /* underlying char is sp1 */
    if (isalign == 2)
        sp->baseline += lift1;  /* adjust baseline */
/* --- extract raster from subraster_t --- */
    rp = sp->image;             /* raster allocated in subraster_t */
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

    if (isfree > 0) {           /* caller wants input freed */
        if (isfree == 1 || isfree > 2) {
            delete_subraster(sp1);
        }
        if (isfree >= 2) {
            delete_subraster(sp2);
        }
    }

end_of_job:
    return sp;
}

/* ============================================================================
 * "Concatanates" subrasters sp1||sp2, leaving both unchanged
 * and returning a newly-allocated subraster_t.
 * 
 * @sp1: subraster_t *  to left-hand subraster_t
 * @sp2: subraster_t *  to right-hand subraster_t
 * @isfree: 1=free sp1 before return, 2=free sp2, 3=free both, 0=free none.
 * 
 * @returns: pointer to constructed subraster_t sp1||sp2 or  NULL for any error
 * ===========================================================================*/
subraster_t *rastcat(subraster_t *sp1, subraster_t *sp2, int isfree)
{
    subraster_t *sp = NULL;       /* returned subraster_t */
    raster_t *rp = NULL;          /* new concatted raster */
    int base1 = sp1->baseline;  /*baseline for left-hand subraster_t */
    int height1 = (sp1->image)->height; /* height for left-hand subraster_t */
    int width1 = (sp1->image)->width;   /* width for left-hand subraster_t */
    int pixsz1 = (sp1->image)->pixsz;   /* pixsz for left-hand subraster_t */
    int type1 = sp1->type;      /* image type for left-hand */
    int base2 = sp2->baseline;  /*baseline for right-hand subraster_t */
    int height2 = (sp2->image)->height; /* height for right-hand subraster_t */
    int width2 = (sp2->image)->width;   /* width for right-hand subraster_t */
    int pixsz2 = (sp2->image)->pixsz;   /* pixsz for right-hand subraster_t */
    int type2 = sp2->type;      /* image type for right-hand */
    int height = 0, width = 0, pixsz = 0, base = 0;     /*concatted sp1||sp2 composite */
    int issmash = (smashmargin != 0 ? 1 : 0);   /* true to "squash" sp1||sp2 */
    int isopaque = (issmash ? 0 : 1);   /* not oppaque if smashing */
    int isblank = 0, nsmash = 0;        /* #cols to smash */
    int oldsmashmargin = smashmargin;   /* save original smashmargin */
    //int oldblanksymspace = blanksymspace;       /* save original blanksymspace */
    //int oldnocatspace = isnocatspace;   /* save original isnocatspace */
    const mathchardef_t *symdef1 = sp1->symdef; /*mathchardef of last left-hand char */
    const mathchardef_t *symdef2 = sp2->symdef; /* mathchardef of right-hand char */
    int class1 = (symdef1 == NULL ? ORDINARY : symdef1->class); /* symdef->class */
    int class2 = (symdef2 == NULL ? ORDINARY : symdef2->class); /* or default */
    int smash1 = (symdef1 != NULL) && (class1 == ORDINARY || class1 == VARIABLE || class1 == OPENING || class1 == CLOSING || class1 == PUNCTION), smash2 = (symdef2 != NULL) && (class2 == ORDINARY || class2 == VARIABLE || class2 == OPENING || class2 == CLOSING || class2 == PUNCTION), space = fontsize / 2 + 1;   /* #cols between sp1 and sp2 */
    int isfrac = (type1 == FRACRASTER && class2 == PUNCTION);   /* sp1 is a \frac and sp2 is punctuation */
/* -------------------------------------------------------------------------
Initialization
-------------------------------------------------------------------------- */
/* --- determine inter-character space from character class --- */
    space = max2(2, (symspace[class1][class2] + fontsize - 3));     /* space */

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
#if DEBUG_LOG_LEVEL >= 999
    fprintf(msgfp,
            "rastcat> space=%d, blanksymspace=%d, isnocatspace=%d\n",
            space, oldblanksymspace, oldnocatspace);
#endif
/* --- determine smash --- */
    if (!isfrac) {  /* don't smash strings or \frac's */
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
#if DEBUG_LOG_LEVEL >= 99
            fprintf(msgfp,
                    "rastcat> maxsmash=%d, margin=%d, nsmash=%d\n",
                    maxsmash, margin, nsmash);
            fprintf(msgfp, "rastcat> type1=%d,2=%d, class1=%d,2=%d\n",
                    type1, type2, (symdef1 == NULL ? -999 : class1),
                    (symdef2 == NULL ? -999 : class2));
#endif
        }
    }

    /* --- determine height, width and baseline of composite raster --- */
    height = max2(base1 + 1, base2 + 1)     /* max height above baseline */
        +max2(height1 - base1 - 1, height2 - base2 - 1);    /*+ max descending below */
    width = width1 + width2 + space - nsmash;       /*add widths and space-smash */
    width = max3(width, width1, width2);

    pixsz = max2(pixsz1, pixsz2);       /* bitmap||bytemap becomes bytemap */
    base = max2(base1, base2);  /* max space above baseline */

#if DEBUG_LOG_LEVEL >= 9999
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
#endif

    /* -------------------------------------------------------------------------
       allocate concatted composite subraster_t
       -------------------------------------------------------------------------- */
    /* --- allocate returned subraster_t (and then initialize it) --- */
#if DEBUG_LOG_LEVEL >= 9999
    fprintf(msgfp, "rastcat> calling new_subraster(%d,%d,%d)\n",
            width, height, pixsz);
#endif
    if ((sp = new_subraster(width, height, pixsz)) == NULL) {   /* failed */
#if DEBUG_LOG_LEVEL >= 1
        fprintf(msgfp, "rastcat> new_subraster(%d,%d,%d) failed\n",
                width, height, pixsz);
#endif
        goto end_of_job;
    }

    /* failed, so quit */
    /* --- initialize subraster_t parameters --- */
    /* sp->type = (!isstring?STRINGRASTER:ASCIISTRING); */
    /*concatted string */
    sp->type =              /*type2; *//*(type1==type2?type2:IMAGERASTER); */
        (type2 != CHARASTER ? type2 :
         (type1 != CHARASTER && type1 != BLANKSIGNAL
          && type1 != FRACRASTER ? type1 : IMAGERASTER));

    sp->symdef = symdef2;       /* rightmost char is sp2 */
    sp->baseline = base;        /* composite baseline */
    sp->size = sp2->size;       /* rightmost char is sp2 */
    if (isblank) {              /* need to propagate blanksignal */
        sp->type = blanksignal; /* may not be completely safe??? */
    }
/* --- extract raster from subraster_t --- */
    rp = sp->image;             /* raster allocated in subraster_t */
/* -------------------------------------------------------------------------
overlay sp1 and sp2 in new composite raster
-------------------------------------------------------------------------- */
#if DEBUG_LOG_LEVEL >= 9999
    fprintf(msgfp,
            "rastcat> calling rastput() to concatanate left||right\n");
#endif
    rastput(rp, sp1->image, base - base1,   /* overlay left-hand */
            max2(0, nsmash - width1), 1);   /* plus any residual smash space */

#if DEBUG_LOG_LEVEL >= 9999
    type_raster(sp->image, msgfp);  /* display composite raster */
#endif
    int fracbase = (isfrac ?        /* baseline for punc after \frac */
                    max2(fraccenterline, base2) : base);    /*adjust baseline or use original */
    rastput(rp, sp2->image, fracbase - base2,       /* overlay right-hand */
            max2(0, width1 + space - nsmash), isopaque);    /* minus any smashed space */
    if (1 && type1 == FRACRASTER    /* we're done with \frac image */
        && type2 != FRACRASTER)     /* unless we have \frac\frac */
        fraccenterline = NOVALUE;   /* so reset centerline signal */
    if (fraccenterline != NOVALUE)  /* sp2 is a fraction */
        fraccenterline += (base - base2);

#if DEBUG_LOG_LEVEL >= 9999
    type_raster(sp->image, msgfp);  /* display composite raster */
#endif
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
 *      and returning a newly-allocated subraster_t,
 *      whose baseline is sp1's if base=1, or sp2's if base=2.
 *      Frees/deletes input sp1 and/or sp2 depending on value
 *      of isfree (0=none, 1=sp1, 2=sp2, 3=both).
 * --------------------------------------------------------------------------
 * Arguments:   sp1 (I)     subraster_t *  to lower subraster_t
 *      sp2 (I)     subraster_t *  to upper subraster_t
 *      base (I)    int containing 1 if sp1 is baseline,
 *              or 2 if sp2 is baseline.
 *      space (I)   int containing #rows blank space inserted
 *              between sp1's image and sp2's image.
 *      iscenter (I)    int containing 1 to center both sp1 and sp2
 *              in stacked array, 0 to left-justify both
 *      isfree (I)  int containing 1=free sp1 before return,
 *              2=free sp2, 3=free both, 0=free none.
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) pointer to constructed subraster_t sp2 atop sp1
 *              or  NULL for any error
 * --------------------------------------------------------------------------
 * Notes:
 * ======================================================================= */
subraster_t *rastack(subraster_t *sp1, subraster_t *sp2, int base, int space,
                   int iscenter, int isfree)
{
    subraster_t *sp = NULL;       /* returned subraster_t */
    raster_t *rp = NULL;          /* new stacked raster in sp */
    int base1 = sp1->baseline,  /* baseline for lower subraster_t */
        height1 = (sp1->image)->height, /* height for lower subraster_t */
        width1 = (sp1->image)->width,   /* width for lower subraster_t */
        pixsz1 = (sp1->image)->pixsz,   /* pixsz for lower subraster_t */
        base2 = sp2->baseline,  /* baseline for upper subraster_t */
        height2 = (sp2->image)->height, /* height for upper subraster_t */
        width2 = (sp2->image)->width,   /* width for upper subraster_t */
        pixsz2 = (sp2->image)->pixsz;   /* pixsz for upper subraster_t */
    int height = 0, width = 0, pixsz = 0, baseline = 0; /*for stacked sp2 atop sp1 */
    const mathchardef_t *symdef1 = sp1->symdef; /* mathchardef of right lower char */
    const mathchardef_t *symdef2 = sp2->symdef; /* mathchardef of right upper char */
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
allocate stacked composite subraster_t (with embedded raster)
-------------------------------------------------------------------------- */
/* --- allocate returned subraster_t (and then initialize it) --- */
    if ((sp = new_subraster(width, height, pixsz)) == NULL) {
        goto end_of_job;        /* failed, so quit */
    }
/* --- initialize subraster_t parameters --- */
    sp->type = IMAGERASTER;     /* stacked rasters */
    sp->symdef = (base == 1 ? symdef1 : (base == 2 ? symdef2 : NULL));  /* symdef */
    sp->baseline = baseline;    /* composite baseline */
    sp->size = (base == 1 ? sp1->size : (base == 2 ? sp2->size : NORMALSIZE));  /*size */
/* --- extract raster from subraster_t --- */
    rp = sp->image;             /* raster embedded in subraster_t */
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
 * Arguments:   tiles (I)   subraster_t *  to array of subraster_t structs
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
raster_t *rastile(subraster_t * tiles, int ntiles)
{
    raster_t *composite = NULL;   /*raster back to caller */
    int width = 0, height = 0, pixsz = 0,       /*width,height,pixsz of composite raster */
        toprow = 9999, rightcol = -999, /* extreme upper-right corner of tiles */
        botrow = -999, leftcol = 9999;  /* extreme lower-left corner of tiles */
    int itile;                  /* tiles[] index */
/* -------------------------------------------------------------------------
run through tiles[] to determine dimensions for composite raster
-------------------------------------------------------------------------- */
/* --- determine row and column bounds of composite raster --- */
    for (itile = 0; itile < ntiles; itile++) {
        subraster_t *tile = &tiles[itile];        /* ptr to current tile */
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
        subraster_t *tile = &(tiles[itile]);      /* ptr to current tile */
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
 * Arguments:   sp1 (I)     subraster_t *  to left-hand raster
 *      sp2 (I)     subraster_t *  to right-hand raster
 * --------------------------------------------------------------------------
 * Returns: ( int )     max #pixels we can smash sp1||sp2,
 *              or "blanksignal" if sp2 intentionally blank,
 *              or 0 for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static int rastsmash(subraster_t * sp1, subraster_t * sp2)
{
    int nsmash = 0;             /* #pixels to smash sp1||sp2 */
    int base1 = sp1->baseline,  /*baseline for left-hand subraster_t */
        height1 = (sp1->image)->height, /* height for left-hand subraster_t */
        width1 = (sp1->image)->width,   /* width for left-hand subraster_t */
        base2 = sp2->baseline,  /*baseline for right-hand subraster_t */
        height2 = (sp2->image)->height, /* height for right-hand subraster_t */
        width2 = (sp2->image)->width;   /* width for right-hand subraster_t */
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
#if DEBUG_LOG_LEVEL >= 99
    fprintf(msgfp, "rastsmash> nsmash=%d, smashmargin=%d\n",
            nsmash, smashmargin);
#if DEBUG_LOG_LEVEL >= 999
    fprintf(msgfp, "rastsmash>left-hand image...\n");
    if (sp1 != NULL)
        type_raster(sp1->image, msgfp); /* left image */
    fprintf(msgfp, "rastsmash>right-hand image...\n");
    if (sp2 != NULL)
        type_raster(sp2->image, msgfp);
#endif
#endif
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
    //const char *expression = term;      /* local ptr to beginning of expression */
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
#if DEBUG_LOG_LEVEL >= 99
                fprintf(msgfp,
                        "rastsmashcheck> only grayspace in %.32s\n",
                        expression);
#endif
            }
            goto skipgray;
        }
    }

    /* restart grayspace check from beginning */
    /* --- don't smash if term begins with a "nosmash" char --- */
    if ((token = strchr(nosmashchars, *term)) != NULL) {
#if DEBUG_LOG_LEVEL >= 99
        fprintf(msgfp, "rastsmashcheck> char %.1s found in %.32s\n",
                token, term);
#endif
        goto end_of_job;
    }
/* -------------------------------------------------------------------------
check for leading no-smash token
-------------------------------------------------------------------------- */
    for (i = 0; (token = nosmashstrs[i]) != NULL; i++) {        /* check each nosmashstr */
        if (strncmp(term, token, strlen(token)) == 0) { /* found a nosmashstr */
#if DEBUG_LOG_LEVEL >= 99
            fprintf(msgfp, "rastsmashcheck> token %s found in %.32s\n",
                    token, term);
#endif
            goto end_of_job;
        }
    }

    isokay = 1;                 /* no problem, so signal okay to smash */
end_of_job:
#if DEBUG_LOG_LEVEL >= 999
    fprintf(msgfp,
            "rastsmashcheck> returning isokay=%d for \"%.32s\"\n",
            isokay, (expression == NULL ? "<no input>" : expression));
#endif
    return isokay;
}

/* ==========================================================================
 * Function:    accent_subraster ( accent, width, height, direction, pixsz )
 * Purpose: Allocate a new subraster_t of width x height
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
 * Returns: ( subraster_t * ) ptr to newly-allocated subraster_t with accent,
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o Some accents have internally-determined dimensions,
 *      and caller should check dimensions in returned subraster_t
 * ======================================================================= */
subraster_t *accent_subraster(int accent, int width, int height,
                                   int direction, int pixsz)
{
    raster_t *rp = NULL;          /*raster containing desired accent */
    subraster_t *sp = NULL;       /* subraster_t returning accent */
    int thickness = 1;          /* line thickness */

    int col0, col1;             /* cols for line */
    int row0, row1;                /* rows for line */
    subraster_t *accsp = NULL;    /*find suitable cmex10 symbol/accent */

    char brace[16];             /*"{" for over, "}" for under, etc */
    int iswidthneg = 0;         /* set true if width<0 arg passed */
    int serifwidth = 0;         /* serif for surd */
    int isBig = 0;              /* true for ==>arrow, false for --> */

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
                line_raster(rp, row0 + serifwidth, 0, row0, serifwidth, thickness);
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
        }                       /* and free subraster_t "envelope" */
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
        }                       /* and free subraster_t "envelope" */
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
        }                       /* and free subraster_t "envelope" */
        break;
        /* --- tilde request --- */
    case TILDEACCENT:
        accsp = (width < 25 ? get_delim("\\sim", -width, CMSY10) : get_delim("~", -width, CMEX10));     /*width search for tilde */
        if (accsp != NULL) {    /* found desired tilde */
            if ((sp = rastack(new_subraster(1, 1, pixsz), accsp, 1, 0, 1, 3))   /*space below */
                !=NULL) {       /* have tilde with space below it */
                rp = sp->image; /* "extract" raster with bitmap */
                free((void *) sp);      /* and free subraster_t "envelope" */
                leftsymdef = NULL;
            }                   /* so \tilde{x}^2 works properly */
        }
        break;
    }
/* -------------------------------------------------------------------------
if we constructed accent raster okay, embed it in a subraster_t and return it
-------------------------------------------------------------------------- */
/* --- if all okay, allocate subraster_t to contain constructed raster --- */
    if (rp != NULL) {           /* accent raster constructed okay */
        if ((sp = new_subraster(0, 0, 0)) == NULL) {           /* and if we fail to allocate */
            delete_raster(rp);  /* free now-unneeded raster */
        } else {                  /* subraster_t allocated okay */
            /* --- init subraster_t parameters, embedding raster in it --- */
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
 * Purpose: Allocate a raster/subraster_t and draw left/right arrow in it
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
 * Returns: ( subraster_t * ) ptr to constructed left/right arrow
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
subraster_t *arrow_subraster(int width, int height, int pixsz, int drctn, int isBig)
{
    subraster_t *arrowsp = NULL;  /* allocate arrow subraster_t */
    int irow, midrow = height / 2;      /* index, midrow is arrowhead apex */
    int icol, thickness = (height > 15 ? 2 : 2);        /* arrowhead thickness and index */
    int pixval = (pixsz == 1 ? 1 : (pixsz == 8 ? 255 : (-1)));  /* black pixel value */
    int ipix;                   /* raster pixmap[] index */
    int npix = width * height;  /* #pixels malloced in pixmap[] */
/* -------------------------------------------------------------------------
allocate raster/subraster_t and draw arrow line
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
        int delta = (width > 6 ? (height > 15 ? 3 : (height > 7 ? 2 : 1)) : 1);
        rule_raster(arrowsp->image, midrow - delta, delta, width - 2 * delta, 1, 0);
        rule_raster(arrowsp->image, midrow + delta, delta, width - 2 * delta, 1, 0);
    }
/* -------------------------------------------------------------------------
construct arrowhead(s)
-------------------------------------------------------------------------- */
    for (irow = 0; irow < height; irow++) {     /* for each row of arrow */
        int delta = abs(irow - midrow); /*arrowhead offset for irow */
        /* --- right arrowhead --- */
        if (drctn >= 0) {       /* right arrowhead wanted */
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
 * Purpose: Allocate a raster/subraster_t and draw up/down arrow in it
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
 * Returns: ( subraster_t * ) ptr to constructed up/down arrow
 *              or NULL for any error.
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
subraster_t *uparrow_subraster(int width, int height, int pixsz, int drctn, int isBig)
{
    subraster_t *arrowsp = NULL;  /* allocate arrow subraster_t */
    int icol, midcol = width / 2;       /* index, midcol is arrowhead apex */
    int irow, thickness = (width > 15 ? 2 : 2); /* arrowhead thickness and index */
    int pixval = (pixsz == 1 ? 1 : (pixsz == 8 ? 255 : -1));    /* black pixel value */
    int ipix;                   /* raster pixmap[] index */
    int npix = width * height;  /* #pixels malloced in pixmap[] */
/* -------------------------------------------------------------------------
allocate raster/subraster_t and draw arrow line
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
int rule_raster(raster_t * rp, int top, int left, int width,
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
    if (rp == NULL) {        /* no raster arg supplied */
        if (workingbox != NULL) {  /* see if we have a workingbox */
            rp = workingbox->image;     /* use workingbox if possible */
        } else {
            return 0;
        }
    }                           /* otherwise signal error to caller */
    if (type == bevel) {         /* remove corners of solid box */
        if (width < 3 || height < 3) {
            type = 0;           /* too small to remove corners */
        }
    }
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
    return isfatal ? (ipix < npix ? 1 : 0) : 1;
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
int line_raster(raster_t *rp, int row0, int col0, int row1, int col1, int thickness)
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
#if DEBUG_LOG_LEVEL >= 29
    fprintf(msgfp,
            "line_raster> row,col0=%d,%d row,col1=%d,%d, thickness=%d\n"
            "\t dy,dx=%3.1f,%3.1f, a=%4.3f, xwidth=%4.3f\n", row0,
            col0, row1, col1, thickness, dy, dx, a, xwidth);
#endif
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
#if DEBUG_LOG_LEVEL >= 29
        fprintf(msgfp, "\t irow=%d, xcol=%4.2f, lo,hicol=%d,%d\n",
                irow, xcol, locol, hicol);
#endif
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
#if 1
    for (icol = min2(col0, col1); icol <= max2(col0, col1); icol++) {       /*each scan line */
        if (!isbar && !isline) {    /* neither vert nor horiz */
            xrow = row0 + ((double) (icol - col0)) * a;     /* "middle" row in icol */
            lorow = max2((int) (xrow - 0.5 * (xheight - 1.0)), 0);  /* topmost row */
            hirow =
                min2((int) (xrow + 0.5 * (xheight - 0.0)),
                     max2(row0, row1));
        }                   /*bot */
#if DEBUG_LOG_LEVEL >= 29
        fprintf(msgfp, "\t icol=%d, xrow=%4.2f, lo,hirow=%d,%d\n",
                icol, xrow, lorow, hirow);
#endif
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
#endif
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
int line_recurse(raster_t * rp, double row0, double col0, double row1,
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
int circle_raster(raster_t * rp, int row0, int col0, int row1,
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
#if DEBUG_LOG_LEVEL >= 39
    fprintf(msgfp, "circle_raster> width,height;quads=%d,%d,%s\n",
            width, height, quads);
#endif
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
#if DEBUG_LOG_LEVEL >= 49
                fprintf(msgfp,
                        "\t...imajor=%d; iminor,quad,irow,icol=%d,%c,%d,%d\n",
                        imajor, iminor, *qptr, irow, icol);
#endif
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
#if DEBUG_LOG_LEVEL >= 49
                    fprintf(msgfp,
                            "\t...iminor=%d; imajor,quad,irow,icol=%d,%c,%d,%d\n",
                            iminor, imajor, *qptr, irow, icol);
#endif
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
int circle_recurse(raster_t * rp, int row0, int col0,
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
int bezier_raster(raster_t * rp, double r0, double c0, double r1,
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
raster_t *border_raster(raster_t * rp, int ntop, int nbot, int isline,
                      int isfree)
{
    raster_t *bp = NULL;          /*raster back to caller */
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
    //if (isstring || (1 && rp->height == 1)) {   /* explicit string signal or infer */
    if (rp->height == 1) {   /* explicit string signal or infer */
        bp = rp;
        goto end_of_job;
    }
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
raster_t *backspace_raster(raster_t * rp, int nback, int *pback, int minspace, int isfree)
{
    raster_t *bp = NULL;          /* raster returned to caller */
    int width = (rp == NULL ? 0 : rp->width);   /* original width of raster */
    int height = (rp == NULL ? 0 : rp->height); /* height of raster */
    int mback = nback;          /* nback adjusted for minspace */
    int newwidth = width;       /* adjusted width after backspace */
    int icol = 0, irow = 0;     /* col,row index */

    if (rp == NULL) {
        goto end_of_job;        /* no input given */
    }

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

/* --- allocate backspaced raster --- */
    bp = new_raster(newwidth, height, rp->pixsz);
    if (bp == NULL) {
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

end_of_job:
#if DEBUG_LOG_LEVEL >= 999
    fprintf(msgfp,          /* diagnostics */
            "backspace_raster> nback=%d,minspace=%d,mback=%d, width:old=%d,new=%d\n",
            nback, minspace, mback, width, newwidth);
#endif
    if (isfree && bp != NULL) {
        delete_raster(rp);      /* free original raster */
    }
    return bp;
}

#if DEBUG_LOG_LEVEL > 0
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
int type_raster(const raster_t * rp, FILE *fp)
{
    static int display_width = 72;      /* max columns for display */
    static char display_chars[] = " 123456789BCDE*";    /* display chars for bytemap */
    char scanline[133];         /* ascii image for one scan line */
    int scan_width;             /* #chars in scan (<=display_width) */
    int irow, locol, hicol = (-1);      /* height index, width indexes */
    const raster_t *bitmaprp = rp;      /* convert .gf to bitmap if needed */
    raster_t *bitmaprp_new = NULL;

/* --- redirect null fp --- */
    if (fp == NULL) {
        fp = stdout;            /* default fp to stdout if null */
    }
#if DEBUG_LOG_LEVEL >= 999
    fprintf(fp,             /* debugging diagnostics */
            "type_raster> width=%d height=%d ...\n",
            rp->width, rp->height);
#endif
/* --- check for ascii string --- */
    if (0 && rp->height == 1) {    /* infer input rp is a string */
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
    if (rp->format == 2 || rp->format == 3) {     /* input is .gf-formatted */
        bitmaprp_new = gftobitmap(rp);      /* so convert it for display */
        bitmaprp = bitmaprp_new;
    }
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
    if (bitmaprp_new != NULL) {
        delete_raster(bitmaprp_new);    /* no longer needed, so free it */
    }
    return 1;
}
#endif

/* ==========================================================================
 * Function:    type_bytemap ( bp, grayscale, width, height, fp )
 * Purpose: Emit an ascii dump representing bp, on fp.
 * --------------------------------------------------------------------------
 * Arguments:   bp (I)      pixbyte_t * to bytemap for which an
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
int type_bytemap(const pixbyte_t * bp, int grayscale, int width, int height, FILE * fp)
{
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

/* --- redirect null fp --- */
    if (fp == (FILE *) NULL)
        fp = stdout;            /* default fp to stdout if null */

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
        if (locol > 0)
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
static raster_t *gftobitmap(const raster_t *gf)
{
    raster_t *rp = NULL;          /* image raster retuned to caller */
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
            } else {
#if DEBUG_LOG_LEVEL >= 1
                fprintf(msgfp,
                        "gftobitmap> found embedded repeat command\n");
#endif
            }
        }
#if DEBUG_LOG_LEVEL >= 10000
        fprintf(stdout,
                "gftobitmap> icount=%d bitval=%d nbits=%d ibit=%d totbits=%d\n",
                icount, bitval, nbits, ibit, totbits);
#endif
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
#if DEBUG_LOG_LEVEL >= 1
                fprintf(msgfp, "gftobitmap> width=%d wbits=%d\n",
                        width, wbits);
#endif
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

static const mathchardef_t *get_symdef(const char *symbol)
{
    const mathchardef_t *symdefs = symtable;    /* table of mathchardefs */
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
#if DEBUG_LOG_LEVEL >= 99
                fprintf(msgfp,
                        "get_symdef> isdisplaystyle=%d, xlated %s to %s\n",
                        isdisplaystyle, symbol, tosym);
#endif
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
            const mathchardef_t *symdef = NULL; /* lookup result with fontnum=0 */
            fontnum = 0;        /*try to look up symbol in any font */
            symdef = get_symdef(symbol);        /* repeat lookup with fontnum=0 */
            fontnum = oldfontnum;       /* reset font family */
            return symdef;
        }                       /* caller gets fontnum=0 lookup */
    }
end_of_job:
#if DEBUG_LOG_LEVEL >= 999
    fprintf(msgfp,
            "get_symdef> symbol=%s matches symtable[%d]=%s (isligature=%d)\n",
            symbol, bestdef,
            (bestdef < 0 ? "NotFound" : symdefs[bestdef].symbol),
            isligature);
#endif

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
    const mathchardef_t *symdefs = symtable;    /* table of mathchardefs */
    const char *ligature = expression;  /*- 1*//* expression ptr */
    const char *symbol = NULL;  /* symdefs[idef].symbol */
    int liglen = strlen(ligature);      /* #chars remaining in expression */
    int iscyrfam = (family == CYR10);   /* true for cyrillic families */
    int idef = 0;               /* symdefs[] index */
    int bestdef = -9999;        /*index of longest matching symdef */
    int maxlen = -9999;         /*length of longest matching symdef */

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
#if DEBUG_LOG_LEVEL >= 999
    fprintf(msgfp,
            "get_ligature> ligature=%.4s matches symtable[%d]=%s\n",
            ligature, bestdef,
            (bestdef < 0 ? "NotFound" : symdefs[bestdef].symbol));
#endif
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
static const chardef_t *get_chardef(const mathchardef_t * symdef, int size)
{
    fontfamily_t *fonts = fonttable;      /* table of font families */
    chardef_t *const*fontdef;          /*tables for desired font, by size */
    chardef_t *gfdata = NULL;     /* chardef for symdef,size */
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

    /* --- look up font family --- */
    for (ifont = 0;; ifont++) {   /* until trailer record found */
        if (fonts[ifont].family < 0) {
#if DEBUG_LOG_LEVEL >= 99
            fprintf(msgfp,
                    "get_chardef> failed to find font family %d\n",
                    family);
#endif
            goto end_of_job;
        } else if (fonts[ifont].family == family) {
            /* found font family */
            break;
        }
    }
/* --- get local copy of table for this family by size --- */
    fontdef = fonts[ifont].fontdef;     /* font by size */

/* --- get font in desired size --- */
    for (;;) {                  /* find size or closest available */
        if (fontdef[size] != NULL) {
            break;              /* found available size */
        } else if (size == NORMALSIZE || sizeinc == 0) {  /* or must be supersampling */
#if DEBUG_LOG_LEVEL >= 99
            fprintf(msgfp,
                    "get_chardef> failed to find font size %d\n",
                    size);
#endif
            goto end_of_job;
        } else {                   /*bump size 1 closer to NORMALSIZE */
            size += sizeinc;    /* see if adjusted size available */
        }
    }
/* --- ptr to chardef struct --- */
    gfdata = &fontdef[size][charnum];       /*ptr to chardef for symbol in size */
/* -------------------------------------------------------------------------
kludge to tweak CMEX10 (which appears to have incorrect descenders)
-------------------------------------------------------------------------- */
    if (family == CMEX10) {     /* cmex10 needs tweak */
        int height = gfdata->toprow - gfdata->botrow + 1;       /*total height of char */
        gfdata->botrow = (isBig ? (-height / 3) : (-height / 4));
        gfdata->toprow = gfdata->botrow + gfdata->image.height;
    }

end_of_job:
#if DEBUG_LOG_LEVEL >= 999
    if (symdef == NULL) {
        fprintf(msgfp, "get_chardef> input symdef==NULL\n");
    } else {
        fprintf(msgfp,
                "get_chardef> requested symbol=\"%s\" size=%d  %s\n",
                symdef->symbol, size,
                (gfdata == NULL ? "FAILED" : "Succeeded"));
    }
#endif
    return gfdata;
}

/* ==========================================================================
 * Function:    get_charsubraster ( symdef, size )
 * Purpose: returns new subraster_t ptr containing
 *      data for symdef at given size
 * --------------------------------------------------------------------------
 * Arguments:   symdef (I)  mathchardef *  corresponding to symbol whose
 *              corresponding chardef subraster_t is wanted
 *      size (I)    int containing 0-5 for desired size
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) pointer to struct defining symbol at size,
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o just wraps a subraster_t envelope around get_chardef()
 * ======================================================================= */
static subraster_t *get_charsubraster(const mathchardef_t *symdef, int size)
{
    const chardef_t *gfdata = NULL;     /* chardef struct for symdef,size */
    subraster_t *sp = NULL;       /* subraster_t containing gfdata */
    raster_t *bitmaprp = NULL;    /* convert .gf-format to bitmap */
    int grayscale = 256;        /* aasupersamp() parameters */

    gfdata = get_chardef(symdef, size);
    if (gfdata != NULL) {
        /* we found it */
        /* allocate subraster_t "envelope" */
        if ((sp = new_subraster(0, 0, 0)) != NULL) {
            const raster_t *image = &gfdata->image;   /* ptr to chardef's bitmap or .gf */
            int format = image->format; /* 1=bitmap, else .gf */
            sp->symdef = symdef;       /* replace NULL with caller's arg */
            sp->size = size;   /*replace default with caller's size */
            sp->baseline = get_baseline(gfdata);       /* get baseline of character */

            if (format == 1) {  /* already a bitmap */
                sp->type = CHARASTER;   /* static char raster */
                sp->image = (raster_t*)image;
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
                raster_t *aa = NULL;      /* antialiased char raster */
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
#if DEBUG_LOG_LEVEL >= 999
    fprintf(msgfp,
            "get_charsubraster> requested symbol=\"%s\" baseline=%d"
            " %s %s\n", symdef->symbol,
            (sp == NULL ? 0 : sp->baseline),
            (sp == NULL ? "FAILED" : "Succeeded"),
            (gfdata == NULL ? "(gfdata=NULL)" : " "));
#endif
    return sp;
}

/* ==========================================================================
 * Function:    get_symsubraster ( symbol, size )
 * Purpose: returns new subraster_t ptr containing
 *      data for symbol at given size
 * --------------------------------------------------------------------------
 * Arguments:   symbol (I)  char *  corresponding to symbol
 *              whose corresponding subraster_t is wanted
 *      size (I)    int containing 0-5 for desired size
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) pointer to struct defining symbol at size,
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o just combines get_symdef() and get_charsubraster()
 * ======================================================================= */

static subraster_t *get_symsubraster(const char *symbol, int size)
{
    subraster_t *sp = NULL; /* subraster_t containing gfdata */
    const mathchardef_t *symdef = NULL;  /* mathchardef lookup for symbol */

    if (symbol != NULL) {        /* user supplied input symbol */
         symdef = get_symdef(symbol);   /*look up corresponding mathchardef */
    }

    if (symdef != NULL) {
        /* lookup succeeded */
        sp = get_charsubraster(symdef, size);  /* so get symbol data in subraster_t */
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
static int get_baseline(const chardef_t *gfdata)
{
    /*toprow = gfdata->toprow, */
    /*TeX top row from .gf file info */
    int botrow = gfdata->botrow;   /*TeX bottom row from .gf file info */
    int height = gfdata->image.height;  /* #rows comprising symbol */

    return (height - 1) + botrow;     /* note: descenders have botrow<0 */
}

/* ==========================================================================
 * Function:    get_delim ( char *symbol, int height, int family )
 * Purpose: returns subraster_t corresponding to the samllest
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
 * Returns: ( subraster_t * ) best matching character available,
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o If height is passed as negative, its absolute value is used
 *      but the best-fit width is searched for (rather than height)
 * ======================================================================= */
subraster_t *get_delim(const char *symbol, int height, int family)
{
    const mathchardef_t *symdefs = symtable;    /* table of mathchardefs */
    subraster_t *sp = NULL;       /* best match char */
    const chardef_t *gfdata = NULL;     /* get chardef struct for a symdef */
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
#if 1
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
#endif
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
#if 0
            for (symptr = lcsymbol; *symptr != '\0'; symptr++) {     /*for each symbol ch */
                if (isalpha(*symptr)) {
                    *symptr = tolower(*symptr);
                }
            }
#endif
            deflen = strlen(lcsymbol);  /* #chars in symbol we're checking */
            if ((symptr = strstr(lcsymbol, unescsymbol)) != NULL) {     /*found caller's sym */
                if ((isoint || strstr(lcsymbol, "oint") == NULL)        /* skip unwanted "oint" */
                    &&(issq || strstr(lcsymbol, "sq") == NULL)) { /* skip unwanted "sq" */
                    if ((deffam == CMSY10 ?     /* CMSY10 or not CMSY10 */
                         symptr == lcsymbol     /* caller's sym is a prefix */
                         && deflen == symlen :  /* and same length */
                         (iscurly || strstr(lcsymbol, "curly") == NULL) &&      /*not unwanted curly */
                         (symptr == lcsymbol    /* caller's sym is a prefix */
                          || symptr == lcsymbol + deflen - symlen))) {  /* or a suffix */
                        for (size = 0; size <= LARGESTSIZE; size++) {   /* check all font sizes */
                            if ((gfdata = get_chardef(&(symdefs[idef]), size)) != NULL) {       /*got one */
                                defheight = gfdata->image.height;       /* height of this character */
                                if (iswidth)    /* width search wanted instead... */
                                    defheight = gfdata->image.width;    /* ...so substitute width */
                                leftsymdef = &symdefs[idef];  /* set symbol class, etc */
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
                }
            }
        }
    }
/* -------------------------------------------------------------------------
construct subraster_t for best fit character, and return it to caller
-------------------------------------------------------------------------- */
    if (bestdef >= 0) {          /* found a best fit for caller */
        sp = get_charsubraster(&(symdefs[bestdef]), bestsize);  /* best subraster_t */
    }
    if ((sp == NULL && height - bigheight > 5) ||bigdef < 0) {           /* delim not in font tables */
        sp = make_delim(symbol, (iswidth ? -height : height));  /* try to build delim */
    }
    if (sp == NULL && bigdef >= 0) {     /* just give biggest to caller */
        sp = get_charsubraster(&(symdefs[bigdef]), bigsize);    /* biggest subraster_t */
    }
#if DEBUG_LOG_LEVEL >= 99
    fprintf(msgfp,
            "get_delim> symbol=%.50s, height=%d family=%d isokay=%s\n",
            (symbol == NULL ? "null" : symbol), height, family,
            (sp == NULL ? "fail" : "success"));
#endif
    return sp;
}

/* ==========================================================================
 * Function:    make_delim ( char *symbol, int height )
 * Purpose: constructs subraster_t corresponding to symbol
 *      exactly as large as height,
 * --------------------------------------------------------------------------
 * Arguments:   symbol (I)  char *  containing, e.g., if symbol="("
 *              for desired delimiter
 *      height (I)  int containing height
 *              for returned character
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) constructed delimiter
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o If height is passed as negative, its absolute value is used
 *      and interpreted as width (rather than height)
 * ======================================================================= */
subraster_t *make_delim(const char *symbol, int height)
{
    subraster_t *sp = NULL;       /* subraster_t returned to caller */
    subraster_t *symtop = NULL, *symbot = NULL, *symmid = NULL, *symbar = NULL;   /* pieces */
    subraster_t *topsym = NULL, *botsym = NULL, *midsym = NULL, *barsym = NULL;   /* +filler */
    int isdrawparen = 0;        /*1=draw paren, 0=build from pieces */
    raster_t *rasp = NULL;        /* sp->image */
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
    int isprealloc = 1;         /*pre-alloc subraster_t, except arrow */
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
    }                           /* don't preallocate subraster_t */
    if (strchr(symbol, '{') != NULL     /* if left { */
        || strchr(symbol, '}') != NULL) {       /* or right } brace wanted */
        isprealloc = 0;
    }
    /* don't preallocate */
    /* --- allocate and initialize subraster_t for constructed delimiter --- */
    if (isprealloc) {           /* pre-allocation wanted */
        if ((sp = new_subraster(width, height, pixsz)) == NULL) {
            goto end_of_job;
        }
        /* --- initialize delimiter subraster_t parameters --- */
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
#if DEBUG_LOG_LEVEL >= 99
    fprintf(msgfp, "make_delim> symbol=%.50s, isokay=%d\n",
            (symbol == NULL ? "null" : symbol), isokay);
#endif
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
    static const char *prefixes[] = {    /*e.g., \big followed by ( */
        "\\big", "\\Big", "\\bigg", "\\Bigg",
        "\\bigl", "\\Bigl", "\\biggl", "\\Biggl",
        "\\bigr", "\\Bigr", "\\biggr", "\\Biggr",
        NULL
    };
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
#if DEBUG_LOG_LEVEL >= 999
    fprintf(msgfp, "texchar> returning token = \"%s\"\n", chartoken);
#endif
    return (char *) expression; /*ptr to 1st non-alpha char */
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

static char *texleft(const char *expression, char *subexpr, int maxsubsz, char *ldelim, char *rdelim)
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
#if DEBUG_LOG_LEVEL >= 99
    fprintf(msgfp, "texleft> ldelim=%s, rdelim=%s, subexpr=%.128s\n",
            (ldelim == NULL ? "none" : ldelim),
            (rdelim == NULL ? "none" : rdelim),
            (subexpr == NULL ? "none" : subexpr));
#endif
    return (char *) pright;
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
#if DEBUG_LOG_LEVEL >= 999
    fprintf(msgfp,
            "isbrace> expression=%.8s, gotbrace=%d (isligature=%d)\n",
            expression, gotbrace, isligature);
#endif
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
        }
    }
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
#if DEBUG_LOG_LEVEL >= 999
            fprintf(msgfp, "mimeprep> offset=%d rhs=\"%s\"\n",
                    (int) (tokptr - expression), tokptr);
#endif
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
#if DEBUG_LOG_LEVEL >= 99
    fprintf(msgfp, "mimeprep> expression=\"\"%s\"\"\n", expression);
#endif
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
char *strchange(int nfirst, char *from, const char *to)
{
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
int strreplace(char *string, const char *from, const char *to, int nreplace)
{
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
char *strwstr(const char *string, const char *substr, const char *white, int *sublen)
{
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
#if DEBUG_LOG_LEVEL >= 999
    fprintf(msgfp,
            "strwstr> str=\"%.72s\" sub=\"%s\" found at offset %d\n",
            string, substr,
            (pfound == NULL ? (-1) : (int) (pfound - string)));
#endif
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
static char *strdetex(char *s, int mode)
{
    static char sbuff[4096];    /* copy of s with no math chars */
/* -------------------------------------------------------------------------
Make a clean copy of s
-------------------------------------------------------------------------- */
/* --- check input --- */
    *sbuff = '\0';              /* initialize in case of error */
    if (isempty(s)) {
        goto end_of_job;        /* no input */
    }
    /* --- start with copy of s --- */
    strninit(sbuff, s, 2048);
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
 * ======================================================================= */
char *strtexchr(const char *string, const char *texchr)
{
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
static char *findbraces(const char *expression, const char *command)
{
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
char *strpspn(const char *s, const char *reject, char *segment)
{
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
 * find first char from s outside () parens (and outside ""'s) and in reject
 * -------------------------------------------------------------------------- */
    while (*ps != '\0') {
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
    }
end_of_job:
    if (segment != NULL) {      /* caller gave us segment */
        if (isempty(reject)) {  /* no reject char */
            segment[min2(seglen, maxseg)] = *ps;
            seglen++;
        }                       /*closing )]} to seg */
        segment[min2(seglen, maxseg)] = '\0';   /* null-terminate the segment */
        trimwhite(segment);
    }                           /* trim leading/trailing whitespace */
    return (char *) ps;
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
int isnumeric(const char *s)
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
int evalterm(const store_t *store, const char *term)
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
static int getstore(const store_t *store, const char *identifier)
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

/* ==========================================================================
 * Function:    rasterize ( expression, size )
 * Purpose: returns subraster_t corresponding to (a valid LaTeX) expression
 *      at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char * to first char of null-terminated
 *              string containing valid LaTeX expression
 *              to be rasterized
 *      size (I)    int containing 0-4 default font size
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to expression,
 *              or NULL for any parsing error.
 * --------------------------------------------------------------------------
 * Notes:     o This is mimeTeX's "main" reusable entry point.  Easy to use:
 *      just call it with a LaTeX expression, and get back a bitmap
 *      of that expression.  Then do what you want with the bitmap.
 * ======================================================================= */
subraster_t *rasterize(const char *expression, int size)
{
    char pretext[512];          /* process preamble, if present */
    char chartoken[MAXSUBXSZ + 1];      /*get subexpression from expr */
    const char *subexpr = chartoken;    /* token may be parenthesized expr */
    const mathchardef_t *symdef;        /*get mathchardef struct for symbol */
    int ligdef;                 /*get symtable[] index for ligature */
    int natoms = 0;             /* #atoms/tokens processed so far */
    subraster_t *sp = NULL;
    subraster_t *prevsp = NULL;   /* raster for current, prev char */
    subraster_t *expraster = NULL;        /* raster returned to caller */
    int family = fontinfo[fontnum].family;      /* current font family */
    int isleftscript = 0;       /* true if left-hand term scripted */
    int wasscripted = 0;        /* true if preceding token scripted */
    int wasdelimscript = 0;     /* true if preceding delim scripted */
    /*int   pixsz = 1; *//*default #bits per pixel, 1=bitmap */
/* --- global values saved/restored at each recursive iteration --- */
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
    subraster_t *oldworkingbox = workingbox;      /* initial working box */
    subraster_t *oldleftexpression = leftexpression;      /*left half rasterized so far */
    double oldunitlength = unitlength;  /* initial unitlength */
    const mathchardef_t *oldleftsymdef = leftsymdef;    /* init oldleftsymdef */

    recurlevel++;               /* wind up one more recursion level */
    leftexpression = NULL;      /* no leading left half yet */
    isreplaceleft = 0;          /* reset replaceleft flag */
    if (1) {
        fraccenterline = NOVALUE;       /* reset \frac baseline signal */
    }
    /* shrinkfactor = shrinkfactors[max2(0,min2(size,LARGESTSIZE))]; *//*set sf */
    shrinkfactor = shrinkfactors[max2(0, min2(size, 16))];      /* have 17 sf's */
    rastlift = 0;               /* reset global rastraise() lift */
#if DEBUG_LOG_LEVEL >= 9
    fprintf(msgfp,
            "rasterize> recursion#%d, size=%d,\n\texpression=\"%s\"\n",
            recurlevel, size,
            (expression == NULL ? "<null>" : expression));
#endif
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
        sp = NULL;              /* no subraster_t yet */
        size = fontsize;        /* in case reset by \tiny, etc */
        /*isleftscript = isdelimscript; *//*was preceding term scripted delim */
        wasscripted = isscripted;       /* true if preceding token scripted */
        wasdelimscript = isdelimscript; /* preceding \right delim scripted */
        if (1) {
            isscripted = 0;     /* no subscripted expression yet */
        }
        isdelimscript = 0;      /* reset \right delim scripted flag */
        /* --- debugging output --- */
#if DEBUG_LOG_LEVEL >= 9
        fprintf(msgfp,
                "rasterize> recursion#%d,atom#%d=\"%s\" (isligature=%d,isleftscript=%d)\n",
                recurlevel, natoms + 1, chartoken, isligature,
                isleftscript);
#endif
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
#if DEBUG_LOG_LEVEL >= 29
                fprintf(msgfp,
                        "rasterize> get_symdef() failed for \"%s\"\n",
                        chartoken);
#endif
                sp = NULL;      /* init to signal failure */
                //if (warninglevel < 1) {
                //    continue;   /* warnings not wanted */
                //}
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
            } else {     /* --- no handler, so just get subraster_t for this character --- */
                if (isligature) {       /* found a ligature */
                    expression = subexprptr + strlen(symdef->symbol);   /*push past it */
                }
                if ((sp = get_charsubraster(symdef, size)) == NULL) {   /* get subraster_t */
                    continue;
                }
            }
        }

        /* --- handle any super/subscripts following symbol or subexpression --- */
        sp = rastlimits(&expression, size, sp);
        isleftscript = (wasscripted || wasdelimscript ? 1 : 0); /*preceding term scripted */
        /* --- debugging output --- */
#if DEBUG_LOG_LEVEL >= 9
        fprintf(msgfp, "rasterize> recursion#%d,atom#%d%s\n",
                recurlevel, natoms + 1,
                (sp == NULL ? " = <null>" : "..."));
        fprintf(msgfp,
                "  isleftscript=%d is/wasscripted=%d,%d is/wasdelimscript=%d,%d\n",
                isleftscript, isscripted, wasscripted,
                isdelimscript, wasdelimscript);
        if (sp != NULL) {
            type_raster(sp->image, msgfp);      /* display raster */
        }
#endif

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
#if DEBUG_LOG_LEVEL >= 999
    fprintf(msgfp, "rasterize> Final recursion level=%d, atom#%d...\n", recurlevel, natoms);
    if (expraster != NULL) {    /* i.e., if natoms>0 */
        type_raster(expraster->image, msgfp);       /* display completed raster */
    }
#endif
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
 * Purpose: parentheses handler, returns a subraster_t corresponding to
 *      parenthesized subexpression at font size
 * --------------------------------------------------------------------------
 * Arguments:   subexpr (I) char **  to first char of null-terminated
 *              string beginning with a LEFTBRACES
 *              to be rasterized
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding leading left{
 *              (unused, but passed for consistency)
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to subexpr,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o This "handler" isn't in the mathchardef symbol table,
 *      but is called directly from rasterize(), as necessary.
 *        o Though subexpr is returned unchanged, it's passed as char **
 *      for consistency with other handlers.  Ditto, basesp is unused
 *      but passed for consistency
 * ======================================================================= */
subraster_t *rastparen(const char **subexpr, int size, subraster_t * basesp)
{
    const char *expression = *subexpr;  /* dereference subexpr to get char* */
    int explen = strlen(expression);    /* total #chars, including parens */
    int isescape = 0;           /* true if parens \escaped */
    int isrightdot = 0;         /* true if right paren is \right. */
    int isleftdot = 0;          /* true if left paren is \left. */
    char left[32], right[32];   /* parens enclosing expresion */
    char noparens[MAXSUBXSZ + 1];       /* get subexpr without parens */
    subraster_t *sp = NULL;       /* rasterize what's between ()'s */
    int isheight = 1;           /*true=full height, false=baseline */
    int height;                 /* height of rasterized noparens[] */
    int baseline;               /* and its baseline */
    int family = /*CMSYEX*/ CMEX10;     /* family for paren chars */
    subraster_t *lp = NULL, *rp = NULL;   /* left and right paren chars */
/* -------------------------------------------------------------------------
rasterize "interior" of expression, i.e., without enclosing parens
-------------------------------------------------------------------------- */
/* --- first see if enclosing parens are \escaped --- */
    if (isthischar(*expression, ESCAPE)) {      /* expression begins with \escape */
        isescape = 1;           /* so set flag accordingly */
    }
    /* --- get expression *without* enclosing parens --- */
    strcpy(noparens, expression);
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
        delete_subraster(sp);   /* if failed, free subraster_t */
        if (lp != NULL)
            free((void *) lp);  /*free left-paren subraster_t envelope */
        if (rp != NULL)
            free((void *) rp);  /*and right-paren subraster_t envelope */
        sp = NULL;              /* signal error to caller */
        goto end_of_job;
    }

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
    if (sp != NULL) {           /* succeeded or ignored \left. */
        if (rp != NULL) {       /* ignore \right. */
            sp = rastcat(sp, rp, 3);    /* concat sp||rp and free sp,rp */
        }
    }

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
 *              just stored in subraster_t)
 *      basesp (I)  subraster_t *  to current character (or
 *              subexpression) immediately preceding script
 *              indicator
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t returned by rastscripts()
 *              or rastdispmath(), or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
subraster_t *rastlimits(const char **expression, int size, subraster_t *basesp)
{
    subraster_t *scriptsp = basesp;       /* and this will become the result */
    subraster_t *dummybase = basesp;      /* for {}_i construct a dummy base */
    int isdisplay = -1;         /* set 1 for displaystyle, else 0 */
    int oldsmashmargin = smashmargin;   /* save original smashmargin */
/* --- to check for \limits or \nolimits preceding scripts --- */
    const char *exprptr = *expression;
    char limtoken[255];         /*check for \limits */
    int toklen = 0;             /* strlen(limtoken) */
    const mathchardef_t *tokdef;        /* mathchardef struct for limtoken */
    int class = (leftsymdef == NULL ? NOVALUE : leftsymdef->class);     /*base sym class */
/* -------------------------------------------------------------------------
determine whether or not to use displaymath
-------------------------------------------------------------------------- */
     scriptlevel++;             /* first, increment subscript level */
    *limtoken = '\0';           /* no token yet */
     isscripted = 0;            /* signal term not (text) scripted */
#if DEBUG_LOG_LEVEL >= 999
    fprintf(msgfp, "rastlimits> scriptlevel#%d exprptr=%.48s\n",
            scriptlevel, (exprptr == NULL ? "null" : exprptr));
#endif
/* --- check for \limits or \nolimits --- */
    skipwhite(exprptr);         /* skip white space before \limits */
    if (!isempty(exprptr)) {    /* non-empty expression supplied */
        exprptr = texchar(exprptr, limtoken);   /* retrieve next token */
    }
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

    smashmargin = oldsmashmargin;       /* reset original smashmargin */
    if (dummybase != basesp)
        delete_subraster(dummybase);    /*free work area */
#if DEBUG_LOG_LEVEL >= 99
    fprintf(msgfp, "rastlimits> scriptlevel#%d returning %s\n",
            scriptlevel, (scriptsp == NULL ? "null" : "..."));
    if (scriptsp != NULL)   /* have a constructed raster */
        type_raster(scriptsp->image, msgfp);        /*display constructed raster */
#endif
    scriptlevel--;              /*lastly, decrement subscript level */
    return scriptsp;
}

/* ==========================================================================
 * Function:    rastscripts ( expression, size, basesp )
 * Purpose: super/subscript handler, returns subraster_t for the leading
 *      scripts in expression, whose base symbol is at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string beginning with a super/subscript,
 *              and returning ptr immediately following
 *              last script character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding leading script
 *              (scripts will be placed relative to base)
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to scripts,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o This "handler" isn't in the mathchardef symbol table,
 *      but is called directly from rasterize(), as necessary.
 * ======================================================================= */
subraster_t *rastscripts(const char **expression, int size, subraster_t * basesp)
{
    char subscript[512];
    char supscript[512];        /* scripts parsed from expression */
    subraster_t *subsp = NULL, *supsp = NULL;     /* rasterize scripts */
    subraster_t *sp = NULL;       /* super- over subscript subraster_t */
    raster_t *rp = NULL;          /* image raster embedded in sp */
    int height = 0, width = 0, baseline = 0;    /* height,width,baseline of sp */
    int subht = 0, subwidth = 0 /* not used --, subln = 0 */ ;  /* height,width,baseline of sub */
    int supht = 0, supwidth = 0 /* not used --, supln = 0 */ ;  /* height,width,baseline of sup */
    int baseht = 0, baseln = 0; /* height,baseline of base */
    int bdescend = 0, sdescend = 0;     /* descender of base, subscript */
    int issub = 0, issup = 0, isboth = 0;       /* true if we have sub,sup,both */
                                        /* not used -- int isbase = 0 */ ;
                                        /* true if we have base symbol */
    int szval = min2(max2(size, 0), LARGESTSIZE);       /* 0...LARGESTSIZE */
    int vbetween = 2;           /* vertical space between scripts */
    int vabove = szval + 1;     /*sup's top/bot above base's top/bot */
    int vbelow = szval + 1;     /*sub's top/bot below base's top/bot */
    int vbottom = szval + 1;    /*sup's bot above (sub's below) bsln */
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
    issub = (subsp != NULL);     /* true if we have subscript */
    issup = (supsp != NULL);     /* true if we have superscript */
    isboth = (issub && issup); /* true if we have both */
    if (!issub && !issup)
         goto end_of_job;       /* quit if we have neither */
/* --- check for leading no-smash chars (if enabled) --- */
    issmashokay = 0;           /* default, don't smash scripts */
    if (smashcheck > 0) {       /* smash checking wanted */
        issmashokay = 1;        /*haven't found a no-smash char yet */
        if (issub) {            /* got a subscript */
            issmashokay = rastsmashcheck(subscript);    /* check if okay to smash */
        }
        if (issmashokay) {      /* clean sub, so check sup */
            if (issup) {        /* got a superscript */
                issmashokay = rastsmashcheck(supscript);        /* check if okay to smash */
            }
        }
    }

    /* -------------------------------------------------------------------------
       get height, width, baseline of scripts,  and height, baseline of base symbol
       -------------------------------------------------------------------------- */
    /* --- get height and width of components --- */
    if (issub) {
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
    sp = new_subraster(width, height, pixsz);
    if (sp == NULL) {
        goto end_of_job;
    }
/* --- initialize subraster_t parameters --- */
    sp->type = IMAGERASTER;     /* set type as constructed image */
    sp->size = size;            /* set given size */
    sp->baseline = baseline;    /* composite scripts baseline */
    rp = sp->image;             /* raster embedded in subraster_t */
/* --- place super/subscripts in new raster --- */
    if (issup)                  /* we have a superscript */
        rastput(rp, supsp->image, 0, 0, 1);     /* it goes in upper-left corner */
    if (issub)                  /* we have a subscript */
        rastput(rp, subsp->image, height - subht, 0, 1);        /*in lower-left corner */

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
 *      sp (I)      subraster_t *  to display math operator
 *              to which super/subscripts will be added
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to sp
 *              plus its scripts, or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o sp returned unchanged if no super/subscript(s) follow it.
 * ======================================================================= */
static subraster_t *rastdispmath(const char **expression, int size,
                               subraster_t * sp) {
    char subscript[512];
    char supscript[512];        /* scripts parsed from expression */
    int issub = 0, issup = 0;   /* true if we have sub,sup */
    subraster_t *subsp = NULL, *supsp = NULL;     /* rasterize scripts */
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
char *strwrap(const char *s, int linelen, int tablen)
{
    static char sbuff[4096];    /* line-wrapped copy of s */
    char *sol = sbuff;          /* ptr to start of current line */
    char tab[32] = "                 "; /* tab string */
    int finalnewline = (lastchar(s) == '\n' ? 1 : 0);   /*newline at end of string? */
    int istab = (tablen > 0 ? 1 : 0);   /* init true to indent first line */
    int iswhite = 0;            /* true if line break on whitespace */
    int rhslen = 0;             /* remaining right hand side length */
    int thislen = 0;            /* length of current line segment */
    int thistab = 0;            /* length of tab on current line */
    int wordlen = 0;            /* length to next whitespace char */
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
#if DEBUG_LOG_LEVEL >= 10000
        fprintf(msgfp, "strwrap> rhslen=%d, sol=\"\"%s\"\"\n", rhslen,
                sol);
#endif
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
    return sbuff;
}

/* ============================================================================
 * Sets loglevel and log output callback function.
 * 
 * @newmsglevel
 * @user_data
 * @error_log_func
 * ===========================================================================*/
void smalltex_setmsg(int newmsglevel, void *user_data, error_log_func_t error_log_func)
{
}

/* ---------------------------------------------------------------------------
 * Output log message.
 * 
 * @loglevel: smaller, more important
 * @fmt: format string like 'printf'
 * ---------------------------------------------------------------------------*/
void smalltex_putlog(int loglevel, const char *fmt, ...)
{
}
