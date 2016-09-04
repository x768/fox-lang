#include "smalltex_pri.h"
#include "mimetex.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>




static int aagridnum(const raster_t *rp, int irow, int icol);
static int aapatternnum(int gridnum);
static int aapatterns(const raster_t * rp, int irow, int icol, int gridnum, int patternum, int grayscale);
static raster_t *aaweights(int width, int height);

/* -------------------------------------------------------------------------
adjustable default values
-------------------------------------------------------------------------- */

/* --- anti-aliasing diagnostics (to help improve algorithm) --- */
static int patternnumcount0[99];
static int patternnumcount1[99];        /*aalookup() counts */
static int ispatternnumcount = 1;       /* true to accumulate counts */


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
 *      bytemap (O) pixbyte_t * to bytemap, calculated
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
int aapnmlookup(const raster_t *rp, pixbyte_t *bytemap, int grayscale)
{
    int width = rp->width, height = rp->height, /* width, height of raster */
        icol = 0, irow = 0,     /* width, height indexes */
        imap = (-1);            /* pixel index = icol + irow*width */
    int bgbitval = 0, fgbitval = 1;     /* background, foreground bitval */
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
    for (irow = 0; irow < height; irow++) {
        for (icol = 0; icol < width; icol++) {
            /* --- local allocations and declarations --- */
            int bitval = 0,     /* value of rp bit at irow,icol */
                isbgdiag = 0, isfgdiag = 0,     /*does pixel border a bg or fg edge */
                aabyteval = 0;  /* antialiased (or unchanged) value */
            /* --- get gridnum and center bit value, init aabyteval --- */
            imap++;             /* first set imap=icol + irow*width */
            gridnum = aagridnum(rp, irow, icol);        /*grid# coding 3x3 grid at irow,icol */
            bitval = (gridnum & 1);     /* center bit set if gridnum odd */
            aabyteval = (pixbyte_t) (bitval == bgbitval ? 0 : grayscale - 1);     /* default aa val */
            bytemap[imap] = (pixbyte_t) (aabyteval);      /* init antialiased pixel */
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
                bytemap[imap] = (pixbyte_t) (aabyteval);  /* set antialiased pixel */
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
                bytemap[imap] = (pixbyte_t) (aabyteval);  /* set antialiased pixel */
#if DEBUG_LOG_LEVEL >= 99
                    fprintf(msgfp,      /*diagnostic output */
                            "%s> irow,icol,imap=%d,%d,%d aawtval=%.4f aabyteval=%d",
                            (isfirstaa ? "aapnmlookup algorithm" :
                             "aapnm"), irow, icol, imap, aawtval,
                            aabyteval);
                    fprintf(msgfp, "\n");   /* no more output */
                    isfirstaa = 0;
#endif
            }
        }
    }
/* -------------------------------------------------------------------------
Back to caller with gray-scale anti-aliased bytemap
-------------------------------------------------------------------------- */
/*end_of_job:*/
    return 1;
}


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
static int aafollowline(const raster_t *rp, int irow, int icol, int direction)
{
    pixbyte_t *bitmap = rp->pixmap;       /* local rp->pixmap ptr */
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
#if DEBUG_LOG_LEVEL >= 99
    fprintf(msgfp,
            "aafollowline> irow,icol,direction=%d,%d,%d, turn=%d\n",
            irow, icol, direction, turn);
#endif
    return turn;
}

/* ==========================================================================
 * Function:    aalowpass ( rp, bytemap, grayscale )
 * Purpose: calculates a lowpass anti-aliased bytemap
 *      for rp->bitmap, with each byte 0...grayscale-1
 * --------------------------------------------------------------------------
 * Arguments:   rp (I)      raster *  to raster whose bitmap
 *              is to be anti-aliased
 *      bytemap (O) pixbyte_t * to bytemap, calculated
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
int aalowpass(raster_t * rp, pixbyte_t * bytemap, int grayscale)
{
    int status = 1;             /* 1=success, 0=failure to caller */
    pixbyte_t *bitmap = (rp == NULL ? NULL : rp->pixmap); /*local rp->pixmap ptr */
    int irow = 0, icol = 0;     /* rp->height, rp->width indexes */
    int weights[9] = {      /* matrix of weights */
        1, 3, 1, 3, 0, 3, 1, 3, 1
    };
    int adjindex[9] = {     /*clockwise from upper-left */
        0, 1, 2, 7, -1, 3, 6, 5, 4
    };
    int totwts = 0;             /* sum of all weights in matrix */
    int isforceavg = 1,         /*force avg black if center black? */
        isminmaxwts = 1,        /*use wts or #pts for min/max test */
        blackscale = 0;         /*(grayscale+1)/4; *//*force black if wgted avg>bs */

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
            for (jrow = irow - 1; jrow <= irow + 1; jrow++) {    /* jrow = irow-1...irow+1 */
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
                }
            }
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
                } else {          /* min/max refer to #adjacent points */
                    if (nadjacent < minadjacent || nadjacent > maxadjacent) {
                        bytemap[ipixel] = 0;
                    }
                }
            }
        }
    }
//end_of_job:
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
 *      bytemap (O) pixbyte_t * to bytemap, calculated
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
int aapnm(raster_t *rp, pixbyte_t * bytemap, int grayscale)
{
    pixbyte_t *bitmap = rp->pixmap;       /* local rp->pixmap ptr */
    int width = rp->width, height = rp->height, /* width, height of raster */
        icol = 0, irow = 0,     /* width, height indexes */
        imap = (-1);            /* pixel index = icol + irow*width */
    int bgbitval = 0, fgbitval = 1;     /* background, foreground bitval */
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
#if 0
    if (weightnum > 2) {
        isbgalias = 1;
    }                       /* simulate bold */
    if (weightnum < 1) {
        isbgonly = 1;
        isfgalias = 0;
    }
#endif
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
    for (irow = 0; irow < height; irow++) {
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
            aabyteval = (pixbyte_t) (bitval == bgbitval ? 0 : grayscale - 1);     /* default aa val */
            bytemap[imap] = (pixbyte_t) (aabyteval);      /* init antialiased pixel */
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
#if 0                   /* true to perform test */
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
#endif
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
                bytemap[imap] = (pixbyte_t) (aabyteval);  /* set antialiased pixel */
#if DEBUG_LOG_LEVEL >= 99
                fprintf(msgfp,      /*diagnostic output */
                        "%s> irow,icol,imap=%d,%d,%d aawtval=%.4f aabyteval=%d\n",
                        (isfirstaa ? "aapnm algorithm" : "aapnm"),
                        irow, icol, imap, aawtval, aabyteval);
                isfirstaa = 0;
#endif
            }
        }
    }

/*end_of_job:*/
    return 1;
}

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
static int aapattern1124(const raster_t * rp, int irow, int icol, int gridnum,
                         int grayscale)
{
    int aaval = -1;             /* antialiased value returned */
    int iscenter = gridnum & 1; /* true if pixel at irow,icol black */
    int patternum = 24;         /* init for pattern#24 default */
    pixbyte_t *bitmap = rp->pixmap;       /* local rp->pixmap ptr */
    int width = rp->width, height = rp->height; /* width, height of raster */
    int jrow = irow, jcol = icol;       /* corner or diagonal row,col */
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
        // fall through
    case 11:
        hdirection = 2;
        vdirection = -1;        /* directions to follow corner */
        if ((jrow = irow + 2) < height) {       /* vert corner below center pixel */
            //vertcornval = getlongbit(bitmap, (icol + jrow * width));
            if ((icol - 1) >= 0)        /* lower diag left of center */
                botdiagval =
                    getlongbit(bitmap, ((icol - 1) + jrow * width));
        }
        if ((jcol = icol + 2) < width) {        /* horz corner right of center */
            //horzcornval = getlongbit(bitmap, (jcol + irow * width));
            if ((irow - 1) >= 0)        /* upper diag above center */
                topdiagval =
                    getlongbit(bitmap, (jcol + (irow - 1) * width));
        }
        break;
    case 18:
        patternum = 11;
        // fall through
    case 22:
        hdirection = -2;
        vdirection = -1;        /* directions to follow corner */
        if ((jrow = irow + 2) < height) {       /* vert corner below center pixel */
            //vertcornval = getlongbit(bitmap, (icol + jrow * width));
            if ((icol + 1) < width)     /* lower diag right of center */
                botdiagval =
                    getlongbit(bitmap, ((icol + 1) + jrow * width));
        }
        if ((jcol = icol - 2) >= 0) {   /* horz corner left of center */
            //horzcornval = getlongbit(bitmap, (jcol + irow * width));
            if ((irow - 1) >= 0)        /* upper diag above center */
                topdiagval = getlongbit(bitmap, (jcol + (irow - 1) * width));
        }
        break;
    case 72:
        patternum = 11;
    case 104:
        hdirection = 2;
        vdirection = 1;         /* directions to follow corner */
        if ((jrow = irow - 2) >= 0) {   /* vert corner above center pixel */
            //vertcornval = getlongbit(bitmap, (icol + jrow * width));
            if ((icol - 1) >= 0)        /* upper diag left of center */
                topdiagval =
                    getlongbit(bitmap, ((icol - 1) + jrow * width));
        }
        if ((jcol = icol + 2) < width) {        /* horz corner right of center */
            //horzcornval = getlongbit(bitmap, (jcol + irow * width));
            if ((irow + 1) < height)    /* lower diag below center */
                botdiagval = getlongbit(bitmap, (jcol + (irow + 1) * width));
        }
        break;
    case 80:
        patternum = 11;
        // fall through
    case 208:
        hdirection = -2;
        vdirection = 1;         /* directions to follow corner */
        if ((jrow = irow - 2) >= 0) {   /* vert corner above center pixel */
            //vertcornval = getlongbit(bitmap, (icol + jrow * width));
            if ((icol + 1) < width)     /* upper diag right of center */
                topdiagval = getlongbit(bitmap, ((icol + 1) + jrow * width));
        }
        if ((jcol = icol - 2) >= 0) {   /* horz corner left of center */
            //horzcornval = getlongbit(bitmap, (jcol + irow * width));
            if ((irow + 1) < height)    /* lower diag below center */
                botdiagval = getlongbit(bitmap, (jcol + (irow + 1) * width));
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
    if (diagval > 1) {
        aaval = (patternum == 24 ? 255 : 191);
    } else {
        hturn = aafollowline(rp, irow, icol, hdirection);
        vturn = aafollowline(rp, irow, icol, vdirection);
        if (vturn * hdirection < 0 && hturn * vdirection < 0) {
            aaval = (patternum == 24 ? 255 : 191);
        } else {
            aaval = grayscale - 1;
        }
    }
/* -------------------------------------------------------------------------
Back to caller with grayscale antialiased value for pixel at irow,icol
-------------------------------------------------------------------------- */
end_of_job:
#if DEBUG_LOG_LEVEL >= 99
    if (aaval >= 0) {           /* have antialiasing result */
            fprintf(msgfp,      /* diagnostic output */
                    "aapattern1124> irow,icol,grid#/2=%d,%d,%d, top,botdiag=%d,%d, "
                    "vert,horzcorn=%d,%d, v,hdir=%d,%d, v,hturn=%d,%d, aaval=%d\n",
                    irow, icol, gridnum / 2, topdiagval, botdiagval,
                    vertcornval, horzcornval, vdirection, hdirection,
                    vturn, hturn, aaval);
    }
#endif
    return aaval;
}

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
static int aapattern19(const raster_t * rp, int irow, int icol, int gridnum,
                       int grayscale)
{
    int aaval = -1;           /* antialiased value returned */
    int iscenter = gridnum & 1; /* true if pixel at irow,icol black */
    int orientation = 1,        /* 1=vertical, 2=horizontal */
        jrow = irow, jcol = icol;       /* middle pixel of *** line */
    int turn1 = 0, turn2 = 0;   /* follow *** line till it turns */
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
    case 7:
        orientation = 2;
        jrow++;
        break;
    case 41:
        orientation = 1;
        jcol++;
        break;
    case 148:
        orientation = 1;
        jcol--;
        break;
    case 224:
        orientation = 2;
        jrow--;
        break;
    }
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
#if DEBUG_LOG_LEVEL >= 99
    if (aaval >= 0)             /* have antialiasing result */
        fprintf(msgfp,      /* diagnostic output */
                "aapattern19> irow,icol,grid#/2=%d,%d,%d, turn+%d,%d=%d,%d, aaval=%d\n",
                irow, icol, gridnum / 2, orientation, -orientation,
                turn1, turn2, aaval);
#endif
    return (aaval);
}

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
static int aapattern20(const raster_t * rp, int irow, int icol, int gridnum,
                       int grayscale)
{
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
    case 14:
        direction = -2;
        jcol1++;
        jrow2++;
        break;
    case 19:
        direction = 2;
        jcol1--;
        jrow2++;
        break;
    case 42:
        direction = 1;
        jrow1++;
        jcol2++;
        break;
    case 73:
        direction = -1;
        jrow1--;
        jcol2++;
        break;
    case 84:
        direction = -1;
        jrow1--;
        jcol2--;
        break;
    case 112:
        direction = 2;
        jcol1--;
        jrow2--;
        break;
    case 146:
        direction = 1;
        jrow1++;
        jcol2--;
        break;
    case 200:
        direction = -2;
        jcol1++;
        jrow2--;
        break;
    }
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
#if DEBUG_LOG_LEVEL >= 99
    if (aaval >= 0)             /* have antialiasing result */
        fprintf(msgfp,      /* diagnostic output */
                "aapattern20> irow,icol,grid#/2=%d,%d,%d, turn%d,%d=%d,%d, aaval=%d\n",
                irow, icol, gridnum / 2, -direction, direction, turn1,
                turn2, aaval);
#endif
    return aaval;
}

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
static int aapattern39(const raster_t * rp, int irow, int icol, int gridnum,
                       int grayscale)
{
    int aaval = -1;           /* antialiased value returned */
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
    case 15:
        direction = -2;
        jcol1++;
        jrow2++;
        break;
    case 23:
        direction = 2;
        jcol1--;
        jrow2++;
        break;
    case 43:
        direction = 1;
        jrow1++;
        jcol2++;
        break;
    case 105:
        direction = -1;
        jrow1--;
        jcol2++;
        break;
    case 212:
        direction = -1;
        jrow1--;
        jcol2--;
        break;
    case 240:
        direction = 2;
        jcol1--;
        jrow2--;
        break;
    case 150:
        direction = 1;
        jrow1++;
        jcol2--;
        break;
    case 232:
        direction = -2;
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
#if DEBUG_LOG_LEVEL >= 99
    if (aaval >= 0)             /* have antialiasing result */
        fprintf(msgfp,      /* diagnostic output */
                "aapattern39> irow,icol,grid#/2=%d,%d,%d, turn%d,%d=%d,%d, aaval=%d\n",
                irow, icol, gridnum / 2, -direction, direction, turn1,
                turn2, aaval);
#endif
    return (aaval);
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
int aapatterns(const raster_t *rp, int irow, int icol, int gridnum, int patternum, int grayscale)
{
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
            case 11:
            case 24:
                aaval = aapattern1124(rp, irow, icol, gridnum, grayscale);
                break;
            case 19:
                aaval = aapattern19(rp, irow, icol, gridnum, grayscale);
            break;
            case 20:
                aaval = aapattern20(rp, irow, icol, gridnum, grayscale);
                break;
            case 39:
                aaval = aapattern39(rp, irow, icol, gridnum, grayscale);
                break;
            /* case 24: if ( (gridnum&1) == 0 ) aaval=0; break; */
            case 29:
                aaval = (iscenter ? grayscale - 1 : 0);
                break;              /* no antialiasing */
        }
    }
    return aaval;
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

static int aagridnum(const raster_t *rp, int irow, int icol)
{
    pixbyte_t *bitmap = rp->pixmap;       /* local rp->pixmap ptr */
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
static int aapatternnum(int gridnum)
{
    int pattern = -1;         /*pattern#, 1-51, for input gridnum */
/* ---
 * pattern number corresponding to input gridnum/2 code
 * ( gridnum/2 strips off gridnum's low bit because it's
 * the same pattern whether or not center pixel is set )
 * --- */
    static const int patternnum[] = {
         1,  3,  4,  7,  3,  8,  7, 19,  4,  7, 11, 24,  9, 23, 20, 39, /*   0- 15 */
         4,  9, 11, 20,  7, 23, 24, 39, 12, 22, 27, 47, 22, 48, 47, 29, /*  16- 31 */
         3,  8,  9, 23, 10, 25, 21, 42,  7, 19, 20, 39, 21, 42, 44, 34, /*  32- 47 */
         9, 26, 28, 41, 21, 50, 49, 30, 22, 43, 45, 33, 40, 32, 31, 13, /*  48- 63 */
         4,  9, 12, 22,  9, 26, 22, 43, 11, 20, 27, 47, 28, 41, 45, 33, /*  64- 79 */
        11, 28, 27, 45, 20, 41, 47, 33, 27, 45, 51, 35, 45, 36, 35, 14, /*  80- 95 */
         7, 23, 22, 48, 21, 50, 40, 32, 24, 39, 47, 29, 49, 30, 31, 13, /*  96-111 */
        20, 41, 45, 36, 44, 38, 31, 15, 47, 33, 35, 14, 31, 15, 16,  5, /* 112-127 */
         3, 10,  9, 21,  8, 25, 23, 42,  9, 21, 28, 49, 26, 50, 41, 30, /* 128-143 */
         7, 21, 20, 44, 19, 42, 39, 34, 22, 40, 45, 31, 43, 32, 33, 13, /* 144-159 */
         8, 25, 26, 50, 25, 46, 50, 37, 23, 42, 41, 30, 50, 37, 38, 17, /* 160-175 */
        23, 50, 41, 38, 42, 37, 30, 17, 48, 32, 36, 15, 32, 18, 15,  6, /* 176-191 */
         7, 21, 22, 40, 23, 50, 48, 32, 20, 44, 45, 31, 41, 38, 36, 15, /* 192-207 */
        24, 49, 47, 31, 39, 30, 29, 13, 47, 31, 35, 16, 33, 15, 14,  5, /* 208-223 */
        19, 42, 43, 32, 42, 37, 32, 18, 39, 34, 33, 13, 30, 17, 15,  6, /* 224-239 */
        39, 30, 33, 15, 34, 17, 13,  6, 29, 13, 14,  5, 13,  6,  5,  2, /* 240-255 */
    -1};
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
static int aalookup(int gridnum)
{
    int grayscale = -1;         /*returned grayscale, init for error */
    int pattern = -1;           /*pattern#, 1-51, for input gridnum */
    int iscenter = gridnum & 1; /*low-order bit set for center pixel */
/* --- gray scales --- */
    enum {
        WHT = 0,
        LGT = 64,
        GRY = 128,
        DRK = 192,
        BLK = 255,
    };

#if 1
/* ---
 * modified aapnm() grayscales (second try)
 * --- */
/* --- grayscale for each pattern when center pixel set/black --- */
    static const int grayscale1[] = {
        -1,                     /* [0] index not used */
        BLK, BLK, BLK, BLK, 242, 230, BLK, BLK, BLK, 160,   /*  1-10 */
        /* BLK,BLK,BLK,BLK,242,230,BLK,BLK,BLK,BLK, *//*  1-10 */
        BLK, BLK, 217, 230, 217, 230, 204, BLK, BLK, 166,   /* 11-20 */
        BLK, BLK, BLK, BLK, BLK, BLK, 178, 166, 204, 191,   /* 21-30 */
        204, BLK, 204, 191, 217, 204, BLK, 191, 178, BLK,   /* 31-40 */
        178, BLK, BLK, 178, 191, BLK, 191, BLK, 178, BLK, 204,      /* 41-51 */
    -1};
/* --- grayscale for each pattern when center pixel not set/white --- */
    static const int grayscale0[] = {
        -1,                     /* [0] index not used */
        WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT,   /*  1-10 */
         64, WHT, WHT, 128, 115, 128, WHT, WHT, WHT, 64,     /* 11-20 */
        /* 51,WHT,WHT,128,115,128,WHT,WHT,WHT, 64, *//* 11-20 */
        WHT, WHT, WHT,  64, WHT, WHT,  76,  64, 102, 89,       /* 21-30 */
        102, WHT, 102, WHT, 115, 102, WHT,  89,  76, WHT,     /* 31-40 */
         76, WHT, WHT,  76,  89, WHT,  89, WHT,  76, WHT, 102,   /* 41-51 */
    -1};
#endif
#if 0
/* ---
 * modified aapnm() grayscales (first try)
 * --- */
/* --- grayscale for each pattern when center pixel set/black --- */
    static const int grayscale1[] = {
        -1,                     /* [0] index not used */
        BLK, BLK, BLK, BLK, 242, 230, GRY, BLK, BLK, BLK,   /*  1-10 */
        BLK, BLK, 217, 230, 217, 230, 204, BLK, BLK, 166,   /* 11-20 */
        BLK, BLK, BLK, BLK, BLK, BLK, BLK, 166, 204, 191,   /* 21-30 */
        204, BLK, 204, BLK, 217, 204, BLK, 191, GRY, BLK,   /* 31-40 */
        178, BLK, BLK, 178, 191, BLK, BLK, BLK, 178, BLK, 204,      /* 41-51 */
    -1};
/* --- grayscale for each pattern when center pixel not set/white --- */
    static const int grayscale0[] = {
        -1,                     /* [0] index not used */
        WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT,   /*  1-10 */
        GRY, WHT, WHT, 128, 115, 128, WHT, WHT, WHT, GRY,   /* 11-20 */
        WHT, WHT, WHT, GRY, WHT, WHT,  76,  64, 102,  89,      /* 21-30 */
        102, WHT, 102, WHT, 115, 102, WHT,  89, GRY, WHT,    /* 31-40 */
         76, WHT, WHT, GRY,  89, WHT,  89, WHT,  76, WHT, 102,  /* 41-51 */
    -1};
#endif
#if 0
/* ---
 * these grayscales _exactly_ correspond to the aapnm() algorithm
 * --- */
/* --- grayscale for each pattern when center pixel set/black --- */
    static const int grayscale1[] = {
        -1,                     /* [0] index not used */
        BLK, BLK, BLK, BLK, 242, 230, BLK, BLK, BLK, BLK,   /*  1-10 */
        BLK, BLK, 217, 230, 217, 230, 204, BLK, BLK, 166,   /* 11-20 */
        BLK, BLK, BLK, BLK, BLK, BLK, 178, 166, 204, 191,   /* 21-30 */
        204, BLK, 204, 191, 217, 204, BLK, 191, 178, BLK,   /* 31-40 */
        178, BLK, BLK, 178, 191, BLK, 191, BLK, 178, BLK, 204,      /* 41-51 */
    -1};
/* --- grayscale for each pattern when center pixel not set/white --- */
    static const int grayscale0[] = {
        -1,                     /* [0] index not used */
        WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT,   /*  1-10 */
         51, WHT, WHT, 128, 115, 128, WHT, WHT, WHT,  64,     /* 11-20 */
        WHT, WHT, WHT,  64, WHT, WHT,  76,  64, 102,  89,       /* 21-30 */
        102, WHT, 102, WHT, 115, 102, WHT,  89,  76, WHT,     /* 31-40 */
         76, WHT, WHT,  76,  89, WHT,  89, WHT,  76, WHT, 102,   /* 41-51 */
    -1};
#endif
/* -------------------------------------------------------------------------
look up grayscale for gridnum
-------------------------------------------------------------------------- */

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
 *      bytemap (O) pixbyte_t * to bytemap, calculated
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
int aalowpasslookup(const raster_t *rp, pixbyte_t *bytemap, int grayscale)
{
    int width = rp->width, height = rp->height; /* width, height of raster */
    int icol = 0, irow = 0, imap = (-1);        /* width, height, bitmap indexes */
    int bgbitval = 0; /*, fgbitval=1 */        /* background, foreground bitval */
    int bitval = 0;             /* value of rp bit at irow,icol */
    int aabyteval = 0;          /* antialiased (or unchanged) value */
    int gridnum = 0;            /* grid# for 3x3 grid at irow,icol */
/* -------------------------------------------------------------------------
generate bytemap by table lookup for each pixel of bitmap
-------------------------------------------------------------------------- */
    for (irow = 0; irow < height; irow++) {
        for (icol = 0; icol < width; icol++) {
            /* --- get gridnum and center bit value, init aabyteval --- */
            gridnum = aagridnum(rp, irow, icol);        /*grid# coding 3x3 grid at irow,icol */
            bitval = (gridnum & 1);     /* center bit set if gridnum odd */
            aabyteval = (pixbyte_t) (bitval == bgbitval ? 0 : grayscale - 1);     /* default aa val */
            imap++;             /* first bump bitmap[] index */
            bytemap[imap] = (pixbyte_t) (aabyteval);      /* init antialiased pixel */
            /* --- look up antialiased value for this grid --- */
            aabyteval = aalookup(gridnum);      /* look up on grid# */
            if (aabyteval >= 0 && aabyteval <= 255) {    /* check for success */
                bytemap[imap] = (pixbyte_t) (aabyteval);  /* init antialiased pixel */
            }
        }
    }
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
int aasupsamp(const raster_t *rp, raster_t **aa, int sf, int grayscale)
{
    int status = 0;             /* 1=success, 0=failure to caller */
    int rpheight = rp->height, rpwidth = rp->width,     /*bitmap raster dimensions */
        heightrem = 0, widthrem = 0,    /* rp+rem is a multiple of shrinkf */
        aaheight = 0, aawidth = 0,      /* supersampled dimensions */
        aapixsz = 8;            /* output pixels are 8-bit bytes */
    int maxaaval = -9999,       /* max grayscale val set in matrix */
        isrescalemax = 1;       /* 1=rescale maxaaval to grayscale */
    int irp = 0, jrp = 0, iaa = 0, jaa = 0, iwt = 0, jwt = 0;   /*indexes, i=width j=height */
    raster_t *aap = NULL;         /* raster for supersampled image */
    static raster_t *aawts = NULL;        /* aaweights() resultant matrix */
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
#if DEBUG_LOG_LEVEL >= 999
        fprintf(msgfp, "aasupsamp> sf=%d, sumwts=%d weights=...\n", sf,
                sumwts);
        type_bytemap((pixbyte_t *) aawts->pixmap, grayscale, aawts->width,
                     aawts->height, msgfp);
#endif
/* --- calculate supersampled height,width and allocate output raster */
    heightrem = rpheight % sf;  /* remainder after division... */
    widthrem = rpwidth % sf;    /* ...by shrinkfactor */
    aaheight = 1 + (rpheight + sf - (heightrem + 1)) / sf;      /* ss height */
    aawidth = 1 + (rpwidth + sf - (widthrem + 1)) / sf; /* ss width */
#if DEBUG_LOG_LEVEL >= 999
        fprintf(msgfp,
                "aasupsamp> rpwid,ht=%d,%d wd,htrem=%d,%d aawid,ht=%d,%d\n",
                rpwidth, rpheight, widthrem, heightrem, aawidth, aaheight);
        fprintf(msgfp, "aasupsamp> dump of original bitmap image...\n");
        type_raster(rp, msgfp);
#endif
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
                if (aafrac >= maxwtfrac) {          /* high weight of sampledblack pts */
                    aaval = grayscale - 1;          /* so set supersampled pt black */
                } else if (aafrac <= minwtfrac) {   /* low weight of sampledblack pts */
                    aaval = 0;      /* so set supersampled pt white */
                } else {            /* rescale calculated weight */
                    aaval =
                        ((sumwts / 2 - 1) +
                         (grayscale - 1) * aaval) / sumwts;
                }
            }
            maxaaval = max2(maxaaval, aaval);   /* largest aaval so far */
#if DEBUG_LOG_LEVEL >= 999
            fprintf(msgfp, "aasupsamp> jrp,irp=%d,%d jaa,iaa=%d,%d"
                    " mrp,nrp=%d,%d aaval=%d\n",
                    jrp, irp, jaa, iaa, mrp, nrp, aaval);
#endif
            if (jaa < aaheight && iaa < aawidth) {      /* bounds check */
                setpixel(aap, jaa, iaa, aaval); /*weighted val in supersamp raster */
            } else {
#if DEBUG_LOG_LEVEL >= 9
               fprintf(msgfp,
                        "aasupsamp> Error: aaheight,aawidth=%d,%d jaa,iaa=%d,%d\n",
                        aaheight, aawidth, jaa, iaa);
#endif
            }
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
#if DEBUG_LOG_LEVEL >= 999
        fprintf(msgfp, "aasupsamp> anti-aliased image...\n");
        type_bytemap((pixbyte_t *) aap->pixmap, grayscale,
                     aap->width, aap->height, msgfp);
#endif
end_of_job:
    return (status);
}

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
static raster_t *aaweights(int width, int height)
{
    raster_t *weights = NULL;
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
int aawtpixel(raster_t * image, int ipixel, raster_t * weights, int rotate)
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
    /* save current rotate as prev */
    /* -------------------------------------------------------------------------
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
