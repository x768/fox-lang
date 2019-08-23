#include "smalltex_pri.h"
#include "mimetex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>


/* --- anti-aliasing parameter values by font weight --- */
static const aaparameters_t aaparams[] = { /* set params by weight */
/* ----------------------------------------------------
 * centerwt adj corner minadj max  fgalias,only,bgalias,only
 * ------------------------------------------------------- */
    {64, 1, 1, 6, 8, 1, 0, 0, 0},       /* 0 = light */
    {CENTERWT, ADJACENTWT, CORNERWT, MINADJACENT, MAXADJACENT, 1, 0, 0, 0},
    {8, 1, 1, 5, 8, 1, 0, 0, 0},        /* 2 = semibold */
    {8, 2, 1, 4, 9, 1, 0, 0, 0} /* 3 = bold */
};




/* ==========================================================================
 * Function:    rastflags ( expression, size, basesp,  flag, value, arg3 )
 * Purpose: sets an internal flag, e.g., for \rm, or sets an internal
 *      value, e.g., for \unitlength=<value>, and returns NULL
 *      so nothing is displayed
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char **  to first char of null-terminated
 *              LaTeX expression (unused/unchanged)
 *      size (I)    int containing base font size (not used,
 *              just stored in subraster_t)
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding "flags" directive
 *              (unused but passed for consistency)
 *      flag (I)    int containing #define'd symbol specifying
 *              internal flag to be set
 *      value (I)   int containing new value of flag
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) NULL so nothing is displayed
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rastflags(const char **expression, int size, const subraster_t *basesp,
                              int flag, int value, int arg3)
{
    char valuearg[1024] = "NOVALUE";
    int argvalue = NOVALUE;
    int isdelta = 0;            /* true if + or - precedes valuearg */
    int valuelen = 0;           /* strlen(valuearg) */
    static int displaystylelevel = -99; /* \displaystyle set at recurlevel */

    switch (flag) {
    default:
        break;
    case ISFONTFAM:
        if (isthischar((*(*expression)), WHITEMATH)) {   /* \rm followed by white */
            (*expression)++;    /* skip leading ~ after \rm */
        }
        fontnum = value;        /* set font family */
        break;
//    case ISSTRING:
//        isstring = value;
//        break;                  /* set string/image mode */
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
            argvalue = value;
        } else {
            *expression = texsubexpr(*expression, valuearg, 1023, "{", "}", 0, 0);
            if (*valuearg != '\0') {
                if (!isalpha(*valuearg)) {
                    if (!isthischar(*valuearg, "?")) {  /*leading ? is query for value */
                        isdelta = isthischar(*valuearg, "+-");  /* leading + or - */
                        if (memcmp(valuearg, "--", 2) == 0) {   /* leading -- signals... */
                            isdelta = 0;
                            strsqueeze(valuearg, 1);
                        }       /* ...not delta */
                        switch (flag) { /* convert to double or int */
                        default:
                            argvalue = atoi(valuearg);
                            break;
                        }
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
                if (*expression != NULL) {      /* ill-formed expression */
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
 * Function:    rastfrac ( expression, size, basesp,  isfrac, arg2, arg3 )
 * Purpose: \frac,\atop handler, returns a subraster_t corresponding to
 *      expression (immediately following \frac,\atop) at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \frac to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \frac
 *              (unused, but passed for consistency)
 *      isfrac (I)  int containing true to draw horizontal line
 *              between numerator and denominator,
 *              or false not to draw it (for \atop).
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to fraction,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
subraster_t *rastfrac(const char **expression, int size, const subraster_t *basesp,
                      int isfrac, int arg2, int arg3)
{
    char numer[MAXSUBXSZ + 1], denom[MAXSUBXSZ + 1];    /* parsed numer, denom */
    subraster_t *numsp = NULL, *densp = NULL;     /*rasterize numer, denom */
    subraster_t *fracsp = NULL;   /* subraster_t for numer/denom */
    int width = 0;              /* width of constructed raster */
    int numheight = 0;          /* height of numerator */
                                /* not used --int baseht = 0, baseln = 0 */ ;
                                /* height,baseline of base symbol */
    /*int   istweak = 1; *//*true to tweak baseline alignment */
    int lineheight = 1;         /* thickness of fraction line */
    int vspace = (size > 2 ? 2 : 1);    /*vertical space between components */

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

/* --- construct raster with numer/denom --- */
    if ((fracsp =
         rastack(densp, numsp, 0, 2 * vspace + lineheight, 1,
                 3)) == NULL) {
        /* failed to construct numer/denom */
        delete_subraster(numsp);
        delete_subraster(densp);
        goto end_of_job;
    }

    /* --- determine width of constructed raster --- */
    width = (fracsp->image)->width;     /*just get width of embedded image */
/* --- initialize subraster_t parameters --- */
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
        rule_raster(fracsp->image, fraccenterline, 0, width, lineheight, 0);
    }
end_of_job:
#if DEBUG_LOG_LEVEL >= 99
    fprintf(msgfp, "rastfrac> returning %s\n",
            (fracsp == NULL ? "null" : "..."));
    if (fracsp != NULL)     /* have a constructed raster */
        type_raster(fracsp->image, msgfp);
#endif
    return fracsp;
}

/* ==========================================================================
 * Function:    rastackrel ( expression, size, basesp,  base, arg2, arg3 )
 * Purpose: \stackrel handler, returns a subraster_t corresponding to
 *      stacked relation
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \stackrel to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \stackrel
 *              (unused, but passed for consistency)
 *      base (I)    int containing 1 if upper/first subexpression
 *              is base relation, or 2 if lower/second is
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to stacked
 *              relation, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rastackrel(const char **expression, int size, const subraster_t *basesp,
                               int base, int arg2, int arg3)
{
    char upper[MAXSUBXSZ + 1], lower[MAXSUBXSZ + 1];    /* parsed upper, lower */
    subraster_t *upsp = NULL, *lowsp = NULL;      /* rasterize upper, lower */
    subraster_t *relsp = NULL;    /* subraster_t for upper/lower */
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
    /* --- rasterize upper, lower --- */
    if (*upper != '\0') {
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
 * Purpose: \log, \lim, etc handler, returns a subraster_t corresponding
 *      to math functions
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \mathfunc to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \mathfunc
 *              (unused, but passed for consistency)
 *      mathfunc (I)    int containing 1=arccos, 2=arcsin, etc.
 *      islimits (I)    int containing 1 if function may have
 *              limits underneath, e.g., \lim_{n\to\infty}
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to mathfunc,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rastmathfunc(const char **expression, int size, const subraster_t *basesp,
                                 int mathfunc, int islimits, int arg3)
{
    char func[MAXTOKNSZ + 1], limits[MAXSUBXSZ + 1];    /*func as {\rm func}, limits */
    char funcarg[MAXTOKNSZ + 1];        /* optional func arg */
    subraster_t *funcsp = NULL, *limsp = NULL;    /*rasterize func,limits */
    subraster_t *mathfuncsp = NULL;       /* subraster_t for mathfunc/limits */
    int limsize = size - 1;     /* font size for limits */
    int vspace = 1;             /*vertical space between components */
/* --- table of function names by mathfunc number --- */
    static const char *funcnames[] = {
        "error",                        /*  0 index is illegal/error bucket */
        "arccos", "arcsin", "arctan",   /*  1 -  3 */
        "arg", "cos", "cosh",           /*  4 -  6 */
        "cot", "coth", "csc",           /*  7 -  9 */
        "deg", "det", "dim",            /* 10 - 12 */
        "exp", "gcd", "hom",            /* 13 - 15 */
        "inf", "ker", "lg",             /* 16 - 18 */
        "lim", "liminf", "limsup",      /* 19 - 21 */
        "ln", "log", "max",             /* 22 - 24 */
        "min", "Pr", "sec",             /* 25 - 27 */
        "sin", "sinh", "sup",           /* 28 - 30 */
        "tan", "tanh",                  /* 31 - 32 */
        /* --- extra mimetex funcnames --- */
        "tr",                           /* 33 */
        "pmod"                          /* 34 */
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
    mathfuncsp->size = size;    /* propagate font size forward */

end_of_job:
    return mathfuncsp;
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
 *      basesp (I)  subraster_t *  to character (or subexpression)
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
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to composite,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rastoverlay(const char **expression, int size, const subraster_t *basesp,
                                int overlay, int offset2, int arg3)
{
    char expr1[512], expr2[512];        /* base, overlay */
    subraster_t *sp1 = NULL, *sp2 = NULL; /*rasterize 1=base, 2=overlay */
    subraster_t *overlaysp = NULL;        /*subraster_t for composite overlay */
    int isalign = 0;            /* true to align baselines */

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
    } else {
        /* use specific built-in overlay */
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
                raster_t *rp = overlaysp->image;  /* raster to be \Not-ed */
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
                raster_t *rp = overlaysp->image;  /* raster to be \sout-ed */
                int width = rp->width, height = rp->height;     /* raster dimensions */
                int baseline = (overlaysp->baseline) - rastlift;        /*skip descenders */
                int midrow =
                    max2(0,
                         min2(height - 1, offset2 + ((baseline + 1) / 2)));
#if 1
                line_raster(rp, midrow, 0, midrow, width - 1, 1);
#endif
            }
            break;
        }
    }
    if (sp2 == NULL)
        goto end_of_job;

    overlaysp = rastcompose(sp1, sp2, offset2, isalign, 3);
end_of_job:
    return overlaysp;
}

/* ==========================================================================
 * Function:    rastspace(expression, size, basesp,  width, isfill, isheight)
 * Purpose: returns a blank/space subraster_t width wide,
 *      with baseline and height corresponding to basep
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char **  to first char of null-terminated
 *              LaTeX expression (unused/unchanged)
 *      size (I)    int containing base font size (not used,
 *              just stored in subraster_t)
 *      basesp (I)  subraster_t *  to character (or subexpression)
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
 * Returns: ( subraster_t * ) ptr to empty/blank subraster_t
 *              or NULL for any error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rastspace(const char **expression, int size, const subraster_t *basesp,
                              int width, int isfill, int isheight)
{
    subraster_t *spacesp = NULL;  /* subraster_t for space */
    raster_t *bp = NULL;          /* for negative space */
    int baseht = 1, baseln = 0; /* height,baseline of base symbol */
    int pixsz = 1;              /*default #bits per pixel, 1=bitmap */
    int isstar = 0, minspace = 0;       /* defaults for negative hspace */
    char widtharg[256];         /* parse for optional {width} */
    int evalue = 0;             /* evaluate [args], {args} */
    subraster_t *rightsp = NULL;  /*rasterize right half of expression */

    if (isfill > 1) {
        isstar = 1;
        isfill = 0;
    }
    /* large fill signals \hspace* */
    if (isfill == NOVALUE) {
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
        if (leftexpression != (subraster_t *) NULL)       /* can't backspace */
            if ((spacesp = new_subraster(0, 0, 0))      /* get new subraster_t for backspace */
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
                    spacesp = (subraster_t *) NULL;
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
       construct blank subraster_t, and return it to caller
       -------------------------------------------------------------------------- */
    /* --- get parameters from base symbol --- */
    if (basesp != (subraster_t *) NULL) { /* we have base symbol for space */
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
    /* --- generate and init space subraster_t --- */
    if (width > 0)              /*make sure we have positive width */
        if ((spacesp = new_subraster(width, baseht, pixsz))     /*generate space subraster_t */
            !=NULL) {           /* and if we succeed... *//* --- ...re-init subraster_t parameters --- */
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
 * Purpose: \\ handler, returns subraster_t corresponding to
 *      left-hand expression preceding \\ above right-hand expression
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \\ to be
 *              rasterized, and returning ptr immediately
 *              to terminating null.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \\
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to expression,
 *              or NULL for any parsing error
 *              (expression ptr unchanged if error occurs)
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rastnewline(const char **expression, int size, const subraster_t *basesp,
                                int arg1, int arg2, int arg3)
{
    subraster_t *newlsp = NULL;   /* subraster_t for both lines */
    subraster_t *rightsp = NULL;  /*rasterize right half of expression */
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
        int newht = (newlsp->image)->height;    /* height of returned subraster_t */
        newlsp->baseline = min2(newht - 1, newht / 2 + 5);      /* guess new baseline */
        isreplaceleft = 1;      /* so set flag to replace left half */
        *expression += strlen(*expression);
    }                           /* and push to terminating null */
    return newlsp;              /* 1st line over 2nd, or null=error */
}

/* ==========================================================================
 * Function:    rastarrow ( expression, size, basesp,  drctn, isBig, arg3 )
 * Purpose: returns left/right arrow subraster_t (e.g., for \longrightarrow)
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char **  to first char of null-terminated
 *              LaTeX expression (unused/unchanged)
 *      size (I)    int containing base font size (not used,
 *              just stored in subraster_t)
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding space, whose baseline
 *              and height params are transferred to space
 *      drctn (I)   int containing +1 for right, -1 for left,
 *              or 0 for leftright
 *      isBig (I)   int containing 0 for ---> or 1 for ===>
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to left/right arrow subraster_t
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
static subraster_t *rastarrow(const char **expression, int size, const subraster_t *basesp,
                              int drctn, int isBig, int arg3)
{
    subraster_t *arrowsp = NULL;  /* subraster_t for arrow */
    char widtharg[256];         /* parse for optional [width] */
    char sub[1024], super[1024];        /* and _^limits after [width] */
    subraster_t *subsp = NULL, *supsp = NULL;     /*rasterize limits */
    subraster_t *spacesp = NULL;  /*space below arrow */
    int width = 10 + 8 * size, height;  /* width, height for \longxxxarrow */
    int islimits = 1;           /*true to handle limits internally */
    int limsize = size - 1;     /* font size for limits */
    int vspace = 1;             /* #empty rows below arrow */
    int pixsz = 1;              /*default #bits per pixel, 1=bitmap */
/* -------------------------------------------------------------------------
construct longleft/rightarrow subraster_t, with limits, and return it to caller
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
    /* replace deafault width *//* --- now parse for limits, and bump expression past it(them) --- */
    if (islimits) {
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
/* --- generate arrow subraster_t --- */
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
/* --- init arrow subraster_t parameters --- */
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
 * Purpose: returns an up/down arrow subraster_t (e.g., for \longuparrow)
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char **  to first char of null-terminated
 *              LaTeX expression (unused/unchanged)
 *      size (I)    int containing base font size (not used,
 *              just stored in subraster_t)
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding space, whose baseline
 *              and height params are transferred to space
 *      drctn (I)   int containing +1 for up, -1 for down,
 *              or 0 for updown
 *      isBig (I)   int containing 0 for ---> or 1 for ===>
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to up/down arrow subraster_t
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
static subraster_t *rastuparrow(const char **expression, int size, const subraster_t *basesp,
                                int drctn, int isBig, int arg3)
{
    subraster_t *arrowsp = NULL;  /* subraster_t for arrow */
    char heightarg[256];        /* parse for optional [height] */
    char sub[1024], super[1024];        /* and _^limits after [width] */
    subraster_t *subsp = NULL, *supsp = NULL;     /*rasterize limits */
    int height = 8 + 2 * size, width;   /* height, width for \longxxxarrow */
    int islimits = 1;           /*true to handle limits internally */
    int limsize = size - 1;     /* font size for limits */
    int pixsz = 1;              /*default #bits per pixel, 1=bitmap */

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
    /* replace deafault height *//* --- now parse for limits, and bump expression past it(them) --- */
    if (islimits) {
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
/* --- generate arrow subraster_t --- */
    if ((arrowsp =
         uparrow_subraster(width, height, pixsz, drctn, isBig)) == NULL)
        goto end_of_job;        /* and quit if we failed */
/* --- init arrow subraster_t parameters --- */
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
        if (subht <= height) {  /* arrow usually taller than script */
            arrowsp->baseline -= deltab;        /* so bottom of script goes here */
        } else {
            subsp->baseline -= deltab;  /* else bottom of arrow goes here */
        }
        arrowsp = rastcat(arrowsp, subsp, 3);
        if (arrowsp == NULL)
            goto end_of_job;
    }

end_of_job:
    arrowsp->baseline = height - 1;     /* reset arrow baseline to bottom */
    return arrowsp;
}



/* ==========================================================================
 * Function:    rastsqrt ( expression, size, basesp,  arg1, arg2, arg3 )
 * Purpose: \sqrt handler, returns a subraster_t corresponding to
 *      expression (immediately following \sqrt) at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \sqrt to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \accent
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to expression,
 *              or NULL for any parsing error
 *              (expression ptr unchanged if error occurs)
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rastsqrt(const char **expression, int size, const subraster_t *basesp,
                             int arg1, int arg2, int arg3)
{
    char subexpr[MAXSUBXSZ + 1];        /*parse subexpr to be sqrt-ed */
    char rootarg[MAXSUBXSZ + 1];        /* optional \sqrt[rootarg]{...} */
    subraster_t *subsp = NULL;    /* rasterize subexpr */
    subraster_t *sqrtsp = NULL;   /* subraster_t with the sqrt */
    subraster_t *rootsp = NULL;   /* optionally preceded by [rootarg] */
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
        subraster_t *fullsp = new_subraster(fullwidth, fullheight, pixsz);
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
 * Purpose: \hat, \vec, \etc handler, returns a subraster_t corresponding
 *      to expression (immediately following \accent) at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \accent to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
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
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to expression,
 *              or NULL for any parsing error
 *              (expression ptr unchanged if error occurs)
 * --------------------------------------------------------------------------
 * Notes:     o Also handles \overbrace{}^{} and \underbrace{}_{} by way
 *      of isabove and isscript args.
 * ======================================================================= */
static subraster_t *rastaccent(const char **expression, int size, const subraster_t *basesp,
                               int accent, int isabove, int isscript)
{
    char subexpr[MAXSUBXSZ + 1];        /*parse subexpr to be accented */
    char *script = NULL;        /* \under,overbrace allow scripts */
    char subscript[MAXTOKNSZ + 1], supscript[MAXTOKNSZ + 1];    /* parsed scripts */
    subraster_t *subsp = NULL, *scrsp = NULL;     /*rasterize subexpr,script */
    subraster_t *accsubsp = NULL; /* stack accent, subexpr, script */
    subraster_t *accsp = NULL;    /*raster for the accent itself */
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
    case DOTACCENT:case DDOTACCENT:
        accheight = (size < 4 ? 3 : 4);  /* default for dots */
        break;
    case VECACCENT:
        vspace = 1;      /* set 1-pixel vertical space */
        accdir = isscript;      /* +1=right,-1=left,0=lr; +10for==> */
        isscript = 0;           /* >>don't<< signal sub/supscript */
        // fall through
    case HATACCENT:
        accheight = 7;   /* default */
        if (subwidth < 10) {
            accheight = 5;      /* unless small width */
        } else if (subwidth > 25) {
            accheight = 9;      /* or large */
        }
        break;
    }
    accheight = min2(accheight, subheight);       /*never higher than accented subexpr */
/* -------------------------------------------------------------------------
construct accent, and construct subraster_t with accent over (or under) subexpr
-------------------------------------------------------------------------- */
/* --- first construct accent --- */
    accsp = accent_subraster(accent, accwidth, accheight, accdir, pixsz);
    if (accsp == NULL)
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

end_of_job:
    if (accsubsp != NULL)       /* initialize subraster_t parameters */
        accsubsp->size = size;  /* propagate font size forward */
    return accsubsp;
}

/* ==========================================================================
 * Function:    rastfont (expression,size,basesp,ifontnum,arg2,arg3)
 * Purpose: \cal{}, \scr{}, \etc handler, returns subraster_t corresponding
 *      to char(s) within {}'s rendered at size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \font to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \accent
 *              (unused, but passed for consistency)
 *      ifontnum (I)    int containing 1 for \cal{}, 2 for \scr{}
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to chars
 *              between {}'s, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rastfont(const char **expression, int size, const subraster_t *basesp,
                             int ifontnum, int arg2, int arg3)
{
    char fontchars[MAXSUBXSZ + 1];      /* chars to render in font */
    char subexpr[MAXSUBXSZ + 1];        /* turn \cal{AB} into \calA\calB */
    char *pfchars = fontchars;
    char fchar = '\0';          /* run thru fontchars one at a time */
    const char *name = NULL;    /* fontinfo[ifontnum].name */
    /* not used--int family = 0: *//* fontinfo[ifontnum].family */
    int istext = 0;             /* fontinfo[ifontnum].istext */
    int class = 0;              /* fontinfo[ifontnum].class */
    subraster_t *fontsp = NULL;   /* rasterize chars in font */
    int oldsmashmargin = smashmargin;   /* turn off smash in text mode */
#if 0
/* --- fonts recognized by rastfont --- */
    static int nfonts = 11;     /* legal font #'s are 1...nfonts */
    static struct {
        const char *name;
        int class;
    } fonts[] = {               /* --- name  class 1=upper,2=alpha,3=alnum,4=lower,5=digit,9=all --- */
        { "\\math", 0 },
        { "\\mathcal", 1 },        /*(1) calligraphic, uppercase */
        { "\\mathscr", 1 },        /*(2) rsfs/script, uppercase */
        { "\\textrm", -1 },        /*(3) \rm,\text{abc} --> {\rm~abc} */
        { "\\textit", -1 },        /*(4) \it,\textit{abc}-->{\it~abc} */
        { "\\mathbb", -1 },        /*(5) \bb,\mathbb{abc}-->{\bb~abc} */
        { "\\mathbf", -1 },        /*(6) \bf,\mathbf{abc}-->{\bf~abc} */
        { "\\mathrm", -1 },        /*(7) \mathrm */
        { "\\cyr", -1 },           /*(8) \cyr */
        { "\\textgreek", -1 },     /*(9) \textgreek */
        { "\\textbfgreek", CMMI10BGR, 1, -1 },     /*(10) \textbfgreek{ab} */
        { "\\textbbgreek", BBOLD10GR, 1, -1 },     /*(11) \textbbgreek{ab} */
        { NULL, 0}
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
#if DEBUG_LOG_LEVEL >= 99
            fprintf(msgfp, "rastfont> \\%s rastflags() for font#%d\n",
                    name, ifontnum);
#endif
            fontsp = rastflags(expression, size, basesp, ISFONTFAM, ifontnum, arg3);
            goto end_of_job;
        }

        /* --- end-of-if(*(*expression)!='{') --- */
        /* ---
           convert \font{abc} --> {\font~abc}
           ---------------------------------- */
        /* --- parse for {fontchars} arg, and bump expression past it --- */
        *expression = texsubexpr(*expression, fontchars, 0, "{", "}", 0, 0);
#if DEBUG_LOG_LEVEL >= 99
        fprintf(msgfp, "rastfont> \\%s fontchars=\"%s\"\n", name,
                fontchars);
#endif
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
#if DEBUG_LOG_LEVEL >= 99
        fprintf(msgfp, "rastfont> \\%s fontchars=\"%s\"\n", name,
                fontchars);
#endif
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
                    if (isupper(fchar & 0xFF)) {
                        isinclass = 1;
                    }
                    break;
                case 2:
                    if (isalpha(fchar & 0xFF)) {
                        isinclass = 1;
                    }
                    break;
                case 3:
                    if (isalnum(fchar & 0xFF)) {
                        isinclass = 1;
                    }
                    break;
                case 4:
                    if (islower(fchar & 0xFF)) {
                        isinclass = 1;
                    }
                    break;
                case 5:
                    if (isdigit(fchar & 0xFF)) {
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
#if DEBUG_LOG_LEVEL >= 99
        fprintf(msgfp, "rastfont> subexpr=\"%s\"\n", subexpr);
#endif
    fontsp = rasterize(subexpr, size);
    if (fontsp == NULL)
        goto end_of_job;        /* and quit if failed */

end_of_job:
    smashmargin = oldsmashmargin;       /* restore smash */
    mathsmashmargin = SMASHMARGIN;      /* this one probably not necessary */
    if (istext && fontsp != NULL)       /* raster contains text mode font */
        fontsp->type = blanksignal;     /* signal nosmash */
    return fontsp;            /* chars rendered in font */
}

/* ==========================================================================
 * Function:    rastrotate ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \rotatebox{degrees}{subexpression} handler, returns subraster_t
 *      containing subexpression rotated by degrees (counterclockwise
 *      if degrees positive)
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \rotatebox to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \rotatebox
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to \rotatebox
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \rotatebox{degrees}{subexpression}
 *        o
 * ======================================================================= */
static subraster_t *rastrotate(const char **expression, int size, const subraster_t *basesp,
                               int arg1, int arg2, int arg3)
{
    char subexpr[MAXSUBXSZ + 1], *degexpr = subexpr;    /* args */
    subraster_t *rotsp = NULL;    /* subraster_t for rotated subexpr */
    raster_t *rotrp = NULL;       /* rotate subraster_t->image 90 degs */
    int baseline = 0;           /* baseline of rasterized image */
    double degrees = 0.0, ipart, fpart; /* degrees to be rotated */
    int idegrees = 0, isneg = 0;        /* positive ipart, isneg=1 if neg */
    int n90 = 0, isn90 = 1;     /* degrees is n90 multiples of 90 */

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
    if (isn90) {                /* rotation by multiples of 90 */
        if (n90 > 0) {          /* do nothing for 0 degrees */
            n90 = 4 - n90;      /* rasrot() rotates clockwise */
            while (n90 > 0) {   /* still have remaining rotations */
                raster_t *nextrp = rastrot(rotrp);        /* rotate raster image */
                if (nextrp == NULL)
                    break;      /* something's terribly wrong */
                delete_raster(rotrp);   /* free previous raster image */
                rotrp = nextrp; /* and replace it with rotated one */
                n90--;
            }                   /* decrement remaining count */
        }
    }
    /* -------------------------------------------------------------------------
       requested rotation not multiple of 90 degrees
       -------------------------------------------------------------------------- */
    if (!isn90) { /* explicitly construct rotation */ ;
    }

    /* not yet implemented */
    /* -------------------------------------------------------------------------
       re-populate subraster_t envelope with rotated image
       -------------------------------------------------------------------------- */
    /* --- re-init various subraster_t parameters, embedding raster in it --- */
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
 * Function:    rastbegin ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \begin{}...\end{}  handler, returns a subraster_t corresponding
 *      to array expression within environment, i.e., rewrites
 *      \begin{}...\end{} as mimeTeX equivalent, and rasterizes that.
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \begin to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \begin
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to array
 *              expression, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rastbegin(const char **expression, int size, const subraster_t *basesp,
                              int arg1, int arg2, int arg3)
{
    char subexpr[MAXSUBXSZ + 1];        /* \begin{} environment params */
    char *exprptr = NULL;
    char *begptr = NULL;
    char *endptr = NULL;
    char *braceptr = NULL;      /* ptrs */
    const char *begtoken = "\\begin{";
    const char *endtoken = "\\end{";    /*tokens we're looking for */
    char *delims = NULL;        /* mdelims[ienviron] */
    subraster_t *sp = NULL;       /* rasterize environment */
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
    if (nbegins > 0) {          /* have nested begins */
        if (blevel < 2) {       /* only need to do this once */
            begptr = subexpr;   /* start at beginning of subexpr */
            while ((begptr = strstr(begptr, begtoken)) != NULL) {       /* have \begin{...} */
                strchange(0, begptr, "{");      /* \begin --> {\begin */
                begptr += strlen(begtoken);
            }                   /* continue past {\begin */
            endptr = subexpr;   /* start at beginning of subexpr */
            while ((endptr = strstr(endptr, endtoken)) != NULL) { /* have \end{...} */
                if ((braceptr = strchr(endptr + 1, '}'))        /* find 1st } following \end{ */
                    == NULL)
                    goto end_of_job;    /* and quit if no } found */
                else {          /* found terminating } */
                    strchange(0, braceptr, "}");        /* \end{...} --> \end{...}} */
                    endptr = braceptr + 1;
                }               /* continue past \end{...} */
            }
        }
    }
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
#if DEBUG_LOG_LEVEL >= 99
    fprintf(msgfp, "rastbegin> subexpr=%s\n", subexpr);
#endif
/* --- rasterize mimeTeX equivalent of \begin{}...\end{} environment --- */
    sp = rasterize(subexpr, size);      /* rasterize subexpr */
end_of_job:
    blevel--;                   /* decrement \begin nesting level */
    return (sp);                /* back to caller with sp or NULL */
}                               /* --- end-of-function rastbegin() --- */


/* ==========================================================================
 * Function:    rastarray ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \array handler, returns a subraster_t corresponding to array
 *      expression (immediately following \array) at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \array to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \array
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to array
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
static subraster_t *rastarray(const char **expression, int size, const subraster_t *basesp,
                              int arg1, int arg2, int arg3)
{
    char subexpr[MAXSUBXSZ + 1];
    char *exprptr;                  /*parse array subexpr */
    char subtok[MAXTOKNSZ + 1];
    /* not used --char *subptr = subtok; */
    /* &,\\ inside { } not a delim */
    char token[MAXTOKNSZ + 1];
    char *tokptr = token;           /* subexpr token to rasterize */
    char *preptr = token;           /*process optional size,lcr preamble */
    const char *coldelim = "&";
    const char *rowdelim = "\\";    /* need escaped rowdelim */
    int maxarraysz = 63;            /* max #rows, cols */
    int justify[65] = {0};          /* -1,0,+1 = l,c,r */
    int hline[65] = {0};            /* hline above row? */
    int vline[65] = {0};            /*vline left of col? */
    int colwidth[65] = {0};         /*widest tokn in col */
    int rowheight[65] = {0};        /* "highest" in row */
    int fixcolsize[65] = {0};       /*1=fixed col width */
    int fixrowsize[65] = {0};       /*1=fixed row height */
    int rowbaseln[65] = {0};        /* baseline for row */
    int vrowspace[65] = {0};        /*extra //[len]space */
    int rowcenter[65] = {0};        /*true = vcenter row */

    /* --- propagate global values across arrays --- */
    static int gjustify[65] = {0};      /* -1,0,+1 = l,c,r */
    static int gcolwidth[65] = {0};     /*widest tokn in col */
    static int growheight[65] = {0};    /* "highest" in row */
    static int gfixcolsize[65] = {0};   /*1=fixed col width */
    static int gfixrowsize[65] = {0};   /*1=fixed row height */
    static int growcenter[65] = {0};    /*true = vcenter row */

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
    subraster_t *toksp[1025];      /* rasterize tokens */
    subraster_t *arraysp = NULL;  /* subraster_t for entire array */
    raster_t *arrayrp = NULL;     /* raster for entire array */
    int rowspace = 2, colspace = 4,     /* blank space between rows, cols */
        hspace = 1, vspace = 1; /*space to accommodate hline,vline */
    int width = 0, height = 0,  /* width,height of array */
        leftcol = 0, toprow = 0;        /*upper-left corner for cell in it */
    const char *hlchar = "\\hline";
    const char *hdchar = "\\hdash";     /* token signals hline */
    char hltoken[1025];         /* extract \hline from token */
    int ishonly = 0, hltoklen, minhltoklen = 3; /*flag, token must be \hl or \hd */
    int isnewrow = 1;           /* true for new row */
    int pixsz = 1;              /*default #bits per pixel, 1=bitmap */
    int evalue = 0;             /* evaluate [arg], {arg} */
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
#if DEBUG_LOG_LEVEL >= 29
    fprintf(msgfp, "rastarray> %.256s\n", subexpr + 2);
#endif
    if (*(subexpr + 2) == '\0') /* couldn't get subexpression */
        goto end_of_job;        /* nothing to do, so quit */
/* -------------------------------------------------------------------------
reset static arrays if main re-entered as daemon (or dll)
-------------------------------------------------------------------------- */
    if (mydaemonlevel != daemonlevel) { /* main re-entered */
        for (icol = 0; icol <= maxarraysz; icol++) {    /* for each array[] index */
            gjustify[icol] = 0;
            gcolwidth[icol] = 0;
            growheight[icol] = 0;
            gfixcolsize[icol] = 0;
            gfixrowsize[icol] = 0;
            growcenter[icol] = 0;
        }
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
#if DEBUG_LOG_LEVEL >= 29
    if (*preptr != '\0')    /* if we have one */
        fprintf(msgfp,
                "rastarray> preamble= \"%.256s\"\nrastarray> preamble: ",
                preptr);
#endif
    irow = icol = 0;            /* init lcr counts */
    while (*preptr != '\0') {   /* check preamble text for lcr */
        char prepchar = *preptr;        /* current preamble character */
        int prepcase = (islower(prepchar) ? 1 : (isupper(prepchar) ? 2 : 0));   /*1,2,or 0 */
        if (irow < maxarraysz && icol < maxarraysz) {
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
        }
#if DEBUG_LOG_LEVEL >= 29
        fprintf(msgfp, " %c[%d]", prepchar,
                (prepcase ==
                 1 ? icol + 1 : (prepcase == 2 ? irow + 1 : 0)));
#endif
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
                //const char *whchars = "?wh";    /* debugging width/height labels */
                preptr = endptr;        /* skip over all digits */
                if (size_digit == 0 || (size_digit >= 3 && size_digit <= 500)) {        /* sanity check */
                    int index;  /* icol,irow...maxarraysz index */
                    if (prepcase == 1) { /* lowercase signifies colwidth */
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
                    } else {       /* uppercase signifies rowheight */
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
                    }
                }               /* --- end-of-if(size>=3&&size<=500) --- */
#if DEBUG_LOG_LEVEL >= 29
                fprintf(msgfp, ":%c=%d/fix#%d", whchars[prepcase],
                        (prepcase ==
                         1 ? colwidth[icol] : rowheight[irow]),
                        (prepcase ==
                         1 ? fixcolsize[icol] : fixrowsize[irow]));
#endif
            }                   /* --- end-of-if(isdigit()) --- */
        }                       /* --- end-of-if(prepcase!=0) --- */
        if (prepcase == 1) {
            icol++;             /* bump col if lowercase lcr */
        } else if (prepcase == 2) {
            irow++;             /* bump row if uppercase BC */
        }
    }                           /* --- end-of-while(*preptr!='\0') --- */
#if DEBUG_LOG_LEVEL >= 29
    if (itoken > 0)         /* if we have preamble */
        fprintf(msgfp, "\n");
#endif
/* -------------------------------------------------------------------------
tokenize and rasterize components  a & b \\ c & d \\ etc  of subexpr
-------------------------------------------------------------------------- */
/* --- rasterize tokens one at a time, and maintain row,col counts --- */
    nrows = 0;                  /* start with top row */
    ncols[nrows] = 0;           /* no tokens/cols in top row yet */
    while (1) {                 /* scan chars till end */
        /* --- local control flags --- */
        int iseox = (*exprptr == '\0'); /* null signals end-of-expression */
        int iseor = iseox;      /* \\ or eox signals end-of-row */
        int iseoc = iseor;      /* & or eor signals end-of-col */
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
#if DEBUG_LOG_LEVEL >= 29
    fprintf(msgfp, "rastarray> %d cols,  widths: ", maxcols);
#endif
    for (icol = 0; icol <= maxcols; icol++) {   /* and for each col */
        width += colwidth[icol];        /*width of this col (0 for maxcols) */
        width += vlinespace(icol);      /*plus space for vline, if present */
#if DEBUG_LOG_LEVEL >= 29
        fprintf(msgfp, " %d=%2d+%d", icol + 1, colwidth[icol],
                (vlinespace(icol)));
#endif
    }
/* --- determine height of array raster --- */
    height = rowspace * (nrows - 1);    /* empty space between rows */
#if DEBUG_LOG_LEVEL >= 29
    fprintf(msgfp, "\nrastarray> %d rows, heights: ", nrows);
#endif
    for (irow = 0; irow <= nrows; irow++) {     /* and for each row */
        height += rowheight[irow];      /*height of this row (0 for nrows) */
        height += vrowspace[irow];      /*plus extra //[len], if present */
        height += hlinespace(irow);     /*plus space for hline, if present */
#if DEBUG_LOG_LEVEL >= 29
        fprintf(msgfp, " %d=%2d+%d", irow + 1, rowheight[irow],
                (hlinespace(irow)));
#endif
    }
/* --- allocate subraster_t and raster for array --- */
#if DEBUG_LOG_LEVEL >= 29
    fprintf(msgfp,
            "\nrastarray> tot width=%d(colspc=%d) height=%d(rowspc=%d)\n",
            width, colspace, height, rowspace);
#endif
    if ((arraysp = new_subraster(width, height, pixsz)) /* allocate new subraster_t */
        == NULL)
        goto end_of_job;        /* quit if failed */
/* --- initialize subraster_t parameters --- */
    arraysp->type = IMAGERASTER;        /* image */
    arraysp->symdef = NULL;     /* not applicable for image */
    arraysp->baseline = min2(height / 2 + 5, height - 1);       /*is a little above center good? */
    arraysp->size = size;       /* size (probably unneeded) */
    arrayrp = arraysp->image;   /* raster embedded in subraster_t */
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
            subraster_t *tsp = toksp[itoken];     /* token that belongs in this cell */
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
        while (--ntokens >= 0)  /* free each token subraster_t */
            if (toksp[ntokens] != NULL) /* if we rasterized this cell */
                delete_subraster(toksp[ntokens]);       /* then free it */
    /* --- return final result to caller --- */
    return (arraysp);
}

/* ==========================================================================
 * Function:    rastleft ( expression, size, basesp, ildelim, arg2, arg3 )
 * Purpose: \left...\right handler, returns a subraster_t corresponding to
 *      delimited subexpression at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I)  char **  to first char of null-terminated
 *              string beginning with a \left
 *              to be rasterized
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding leading left{
 *              (unused, but passed for consistency)
 *      ildelim (I) int containing ldelims[] index of
 *              left delimiter
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to subexpr,
 *              or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rastleft(const char **expression, int size, const subraster_t * basesp,
                             int ildelim, int arg2, int arg3)
{
    subraster_t *sp = NULL;       /*rasterize between \left...\right */
    subraster_t *lp = NULL, *rp = NULL;   /* left and right delim chars */
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
        "unused", ".",      /* 1   for \left., \right. */
        "(", ")",           /* 2,3 for \left(, \right) */
        "\\{", "\\}",       /* 4,5 for \left\{, \right\} */
        "[", "]",           /* 6,7 for \left[, \right] */
        "<", ">",           /* 8,9 for \left<, \right> */
        "|", "\\|",         /* 10,11 for \left,\right |,\| */
        NULL
    };
/* --- recognized operator delimiters --- */
    static const char *opdelims[] = {   /* operator delims from cmex10 */
        "int", "sum", "prod", "cup", "cap", "dot", "plus", "times", "wedge", "vee", NULL
    };
/* --- delimiter xlation --- */
    static const char *xfrom[] =        /* xlate any delim suffix... */
    {
        "\\|",              /* \| */
        "\\{",              /* \{ */
        "\\}",              /* \} */
        "\\lbrace",         /* \lbrace */
        "\\rbrace",         /* \rbrace */
        "\\langle",         /* \langle */
        "\\rangle",         /* \rangle */
        NULL
    };
    static const char *xto[] =  /* ...to this instead */
    {
        "=",                /* \| to = */
        "{",                /* \{ to { */
        "}",                /* \} to } */
        "{",                /* \lbrace to { */
        "}",                /* \rbrace to } */
        "<",                /* \langle to < */
        ">",                /* \rangle to > */
        NULL
    };
/* --- non-displaystyle delimiters --- */
    static const char *textdelims[] =   /* these delims _aren't_ display */
    {
        "|", "=", "(", ")", "[", "]", "<", ">", "{", "}", "dbl",        /* \lbrackdbl and \rbrackdbl */
        NULL
    };
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
#if DEBUG_LOG_LEVEL >= 99
    fprintf(msgfp,
            "rastleft> left=\"%s\" right=\"%s\" subexpr=\"%s\"\n",
            ldelim, rdelim, subexpr);
#endif
/* --- rasterize subexpression --- */
    if ((sp = rasterize(subexpr, size)) == NULL) {
        goto end_of_job;        /* quit if failed */
    }
    height = sp->image->height; /* height of subexpr raster */
    rheight = height + margin;  /*default rheight as subexpr height */

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
#if 0
    if ((lp == NULL && !isleftdot)  /* check that we got left( */
        ||(rp == NULL && !isrightdot)) {    /* and right) if needed */
        if (lp != NULL)
            free((void *) lp);      /* free \left-delim subraster_t */
        if (rp != NULL)
            free((void *) rp);      /* and \right-delim subraster_t */
        if (0) {
            delete_subraster(sp);   /* if failed, free subraster_t */
            sp = (subraster_t *) NULL;
        }                   /* signal error to caller */
        goto end_of_job;
    }
#endif

    /* -------------------------------------------------------------------------
       concat  lp || sp || rp  components
       -------------------------------------------------------------------------- */
    /* --- concat lp||sp||rp to obtain final result --- */
    if (lp != NULL) {           /* ignore \left. */
        sp = rastcat(lp, sp, 3);        /* concat lp||sp and free sp,lp */
    }
    if (sp != NULL) {           /* succeeded or ignored \left. */
        if (rp != NULL) {       /* ignore \right. */
            sp = rastcat(sp, rp, 3);    /* concat sp||rp and free sp,rp */
        }
    }

end_of_job:
    isdelimscript = isrightscript;      /* signal if right delim scripted */
    return sp;
}

/* ==========================================================================
 * Function:    rastmiddle ( expression, size, basesp,  arg1, arg2, arg3 )
 * Purpose: \middle handler, returns subraster_t corresponding to
 *      entire expression with \middle delimiter(s) sized to fit.
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \middle to be
 *              rasterized, and returning ptr immediately
 *              to terminating null.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \middle
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to expression,
 *              or NULL for any parsing error
 *              (expression ptr unchanged if error occurs)
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rastmiddle(const char **expression, int size, const subraster_t *basesp,
                               int arg1, int arg2, int arg3)
{
    subraster_t *sp = NULL, *subsp[32];   /*rasterize \middle subexpr's */
    const char *exprptr = *expression;  /* local copy of ptr to expression */
    char delim[32][132];        /* delimiters following \middle's */
    char subexpr[MAXSUBXSZ + 1];
    char *subptr = NULL;        /*subexpression between \middle's */
    int height = 0, habove = 0, hbelow = 0;     /* height, above & below baseline */
    int idelim, ndelims = 0;    /* \middle count (max 32) */
    int family = CMSYEX;        /* delims from CMSY10 or CMEX10 */

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
        /* max #rows below baseline *//* --- get delimter after \middle --- */
        skipwhite(exprptr);
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
            subraster_t *delimsp = get_delim(delim[idelim], height, family);
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

end_of_job:
#if 0
    for (idelim = 1; idelim <= ndelims; idelim++) { /* free subsp[]'s (not 0) */
        if (subsp[idelim] != NULL) {        /* have allocated subraster_t */
            delete_subraster(subsp[idelim]);        /* so free it */
        }
    }
#endif
    if (sp != NULL) {           /* returning entire expression */
        int newht = (sp->image)->height;        /* height of returned subraster_t */
        sp->baseline = min2(newht - 1, newht / 2 + 5);  /* guess new baseline */
        isreplaceleft = 1;      /* set flag to replace left half */
        *expression += strlen(*expression);
    }                           /* and push to terminating null */
    return (sp);
}
/* ==========================================================================
 * Function:    rastpicture ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \picture handler, returns subraster_t corresponding to picture
 *      expression (immediately following \picture) at font size
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \picture to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \picture
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to picture
 *              expression, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \picture(width,height){(x,y){pic_elem}~(x,y){pic_elem}~etc}
 *        o 
 * ======================================================================= */
static subraster_t *rastpicture(const char **expression, int size, const subraster_t *basesp,
                                int arg1, int arg2, int arg3)
{
    char picexpr[2049], *picptr = picexpr;      /* picture {expre} */
    char putexpr[256], *putptr, *multptr;        /*[multi]put (x,y[;xinc,yinc;num]) */
    char pream[96], *preptr;     /* optional put preamble */
    char picelem[1025];          /* picture element following put */
    subraster_t *picelemsp = NULL;        /* rasterize picture elements */
    subraster_t *picturesp = NULL;      /* subraster_t for entire picture */
    subraster_t *oldworkingbox = workingbox;    /* save working box on entry */
    raster_t *picturerp = NULL;   /* raster for entire picture */
    int pixsz = 1;              /* pixels are one bit each */
    double x = 0.0, y = 0.0;    /* x,y-coords for put,multiput */
    double xinc = 0.0, yinc = 0.0; /* x,y-incrementss for multiput */
    int width = 0, height = 0;  /* #pixels width,height of picture */
    int ewidth = 0, eheight = 0;        /* pic element width,height */
    int ix = 0, xpos = 0, iy = 0, ypos = 0;     /* mimeTeX x,y pixel coords */
    int num = 1, inum;          /* number reps, index of element */
    int iscenter = 0;           /* center or lowerleft put position */
    int *oldworkingparam = workingparam;        /* save working param on entry */
    int origin = 0;             /* x,yinc ++=00 +-=01 -+=10 --=11 */

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
allocate subraster_t and raster for complete picture
-------------------------------------------------------------------------- */
/* --- sanity check on width,height args --- */
    if (width < 2 || width > 600 || height < 2 || height > 600)
        goto end_of_job;
/* --- allocate and initialize subraster_t for constructed picture --- */
    picturesp = new_subraster(width, height, pixsz);
    if (picturesp == NULL)
        goto end_of_job;        /* quit if failed */
    workingbox = picturesp;     /* set workingbox to our picture */
/* --- initialize picture subraster_t parameters --- */
    picturesp->type = IMAGERASTER;      /* image */
    picturesp->symdef = NULL;   /* not applicable for image */
    picturesp->baseline = height / 2 + 2;       /* is a little above center good? */
    picturesp->size = size;     /* size (probably unneeded) */
    picturerp = picturesp->image;       /* raster embedded in subraster_t */
#if DEBUG_LOG_LEVEL >= 29
    fprintf(msgfp, "picture> width,height=%d,%d\n", width, height);
#endif
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
        while (*picptr != '\0') { /* skip invalid chars preceding ( */
            if (*picptr == '(') {
                break;          /* found opening ( */
            } else {
                picptr++;       /* else skip invalid char */
            }
        }
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
        for (preptr = pream;; preptr++) { /* examine each preamble char */
            if (*preptr == '\0') {
                break;          /* end-of-preamble signalled */
            } else {
                switch (tolower(*preptr)) {     /* check lowercase preamble char */
                default:
                    break;      /* unrecognized flag */
                case 'c':
                    iscenter = 1;
                    break;      /* center pic_elem at x,y coords */
                }
            }
        }
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
#if DEBUG_LOG_LEVEL >= 29
        fprintf(msgfp,
                "picture> pream;x,y;xinc,yinc;num=\"%s\";%.2f,%.2f;%.2f,%.2f;%d\n",
                pream, x, y, xinc, yinc, num);
#endif
        /* -----------------------------------------------------------------------
           now obtain {...} picture element following [multi]put, and rasterize it
           ------------------------------------------------------------------------ */
        /* --- parse for {...} picture element and bump picptr past it --- */
        picptr = texsubexpr(picptr, picelem, 1023, "{", "}", 0, 0);
        if (*picelem == '\0')
            goto end_of_job;    /* couldn't get {pic_elem} */
#if DEBUG_LOG_LEVEL >= 29
        fprintf(msgfp, "picture> picelem=\"%.50s\"\n", picelem);
#endif
        /* --- rasterize picture element --- */
        origin = 0;             /* init origin as working param */
        workingparam = &origin; /* and point working param to it */
        picelemsp = rasterize(picelem, size);   /* rasterize picture element */
        if (picelemsp == NULL)
            continue;           /* failed to rasterize, skip elem */
        ewidth = (picelemsp->image)->width;     /* width of element, in pixels */
        eheight = (picelemsp->image)->height;   /* height of element, in pixels */
        if (origin == 55) {
            iscenter = 1;       /* origin set to (.5,.5) for center */
        }
#if DEBUG_LOG_LEVEL >= 29
        fprintf(msgfp,
                "picture> ewidth,eheight,origin,num=%d,%d,%d,%d\n",
                ewidth, eheight, origin, num);
#if DEBUG_LOG_LEVEL >= 999
            type_raster(picelemsp->image, msgfp);
#endif
#endif
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
#if DEBUG_LOG_LEVEL >= 29
            fprintf(msgfp,
                    "picture> inum,x,y,ix,iy,xpos,ypos=%d,%.2f,%.2f,%d,%d,%d,%d\n",
                    inum, x, y, ix, iy, xpos, ypos);
#endif
            /* --- embed token raster at xpos,ypos, and quit if out-of-bounds --- */
            if (!rastput(picturerp, picelemsp->image, ypos, xpos, 0))
                break;
            /* --- apply increment --- */
            if (xinc == 0. && yinc == 0.)
                break;          /* quit if both increments zero */
            x += xinc;
            y += yinc;          /* increment coords for next iter */
        }                       /* --- end-of-for(inum) --- */
        /* --- free picture element subraster_t after embedding it in picture --- */
        delete_subraster(picelemsp);    /* done with subraster_t, so free it */
    }                           /* --- end-of-while(*picptr!=0) --- */

end_of_job:
    workingbox = oldworkingbox; /* restore original working box */
    workingparam = oldworkingparam;     /* restore original working param */
    return picturesp;
}

/* ==========================================================================
 * Function:    rastline ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \line handler, returns subraster_t corresponding to line
 *      parameters (xinc,yinc){xlen}
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \line to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \line
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to line
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \line(xinc,yinc){xlen}
 *        o if {xlen} not given, then it's assumed xlen = |xinc|
 * ======================================================================= */
static subraster_t *rastline(const char **expression, int size, const subraster_t *basesp,
                             int arg1, int arg2, int arg3)
{
    char linexpr[257], *xptr = linexpr; /*line(xinc,yinc){xlen} */
    subraster_t *linesp = NULL;   /* subraster_t for line */
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
#if DEBUG_LOG_LEVEL >= 29
    fprintf(msgfp,
            "rastline> width,height,origin;x,yinc=%d,%d,%d;%g,%g\n",
            width, height, origin, xinc, yinc);
#endif

/* --- sanity check on width,height,thickness args --- */
    if (width < 1 || width > 600
        || height < 1 || height > 600 || thickness < 1 || thickness > 25)
        goto end_of_job;
/* --- allocate and initialize subraster_t for constructed line --- */
    if ((linesp = new_subraster(rwidth, rheight, pixsz))        /* alloc new subraster_t */
        == NULL)
        goto end_of_job;
/* --- initialize line subraster_t parameters --- */
    linesp->type = IMAGERASTER; /* image */
    linesp->symdef = NULL;      /* not applicable for image */
    linesp->baseline = height / 2 + 2   /* is a little above center good? */
        + (rheight - height) / 2;       /* account for line thickness too */
    linesp->size = size;        /* size (probably unneeded) */

    line_raster(linesp->image,  /* embedded raster image */
                (istop ? 0 : height - 1),       /* row0, from bottom or top */
                (isright ? width - 1 : 0),      /* col0, from left or right */
                (istop ? height - 1 : 0),       /* row1, to top or bottom */
                (isright ? 0 : width - 1),      /* col1, to right or left */
                thickness);     /* line thickness (usually 1 pixel) */

  end_of_job:
    if (workingparam != NULL)   /* caller wants origin */
        *workingparam = origin; /* return origin corner to caller */
    return linesp;
}

/* ==========================================================================
 * Function:    rastrule ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \rule handler, returns subraster_t corresponding to rule
 *      parameters [lift]{width}{height}
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \rule to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \rule
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to rule
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \rule[lift]{width}{height}
 *        o if [lift] not given, then bottom of rule on baseline
 *        o if width=0 then you get an invisible strut 1 (one) pixel wide
 * ======================================================================= */
static subraster_t *rastrule(const char **expression, int size, const subraster_t *basesp,
                             int arg1, int arg2, int arg3)
{
    char rulexpr[257];          /* rule[lift]{wdth}{hgt} */
    subraster_t *rulesp = NULL;   /* subraster_t for rule */
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

/* --- sanity check on width,height,thickness args --- */
    if (rwidth < 1 || rwidth > 600 || rheight < 1 || rheight > 600)
        goto end_of_job;
/* --- allocate and initialize subraster_t for constructed rule --- */
    if ((rulesp = new_subraster(rwidth, rheight, pixsz))        /* alloc new subraster_t */
        == NULL)
        goto end_of_job;        /* quit if failed */
/* --- initialize line subraster_t parameters --- */
    rulesp->type = IMAGERASTER; /* image */
    rulesp->symdef = NULL;      /* not applicable for image */
    rulesp->baseline = rheight - 1 + (lift >= 0 ? 0 : lift);    /*adjust baseline for lift */
    rulesp->size = size;        /* size (probably unneeded) */

    rule_raster(rulesp->image,  /* embedded raster image */
                (-lift < height ? 0 : rheight - height),        /* topmost row for top-left corner */
                0,              /* leftmost col for top-left corner */
                width,          /* rule width */
                height,         /* rule height */
                (width > 0 ? 0 : 4));   /* rule type */

end_of_job:
    return rulesp;
}

/* ==========================================================================
 * Function:    rastcircle ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \circle handler, returns subraster_t corresponding to ellipse
 *      parameters (xdiam[,ydiam])
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \circle to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \circle
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to ellipse
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \circle(xdiam[,ydiam])
 *        o
 * ======================================================================= */
static subraster_t *rastcircle(const char **expression, int size, const subraster_t *basesp,
                               int arg1, int arg2, int arg3)
{
    char circexpr[512], *xptr = circexpr;       /*circle(xdiam[,ydiam]) */
    char *qptr = NULL, quads[256] = "1234";     /* default to draw all quadrants */
    double theta0 = 0.0, theta1 = 0.0;  /* ;theta0,theta1 instead of ;quads */
    subraster_t *circsp = NULL;   /* subraster_t for ellipse */
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
#if DEBUG_LOG_LEVEL >= 29
    fprintf(msgfp, "rastcircle> width,height;quads=%d,%d,%s\n",
            width, height, (qptr == NULL ? "default" : qptr));
#endif
/* -------------------------------------------------------------------------
allocate subraster_t and raster for complete picture
-------------------------------------------------------------------------- */
/* --- sanity check on width,height args --- */
    if (width < 1 || width > 600 || height < 1 || height > 600)
        goto end_of_job;
/* --- allocate and initialize subraster_t for constructed ellipse --- */
    if ((circsp = new_subraster(width, height, pixsz))  /* allocate new subraster_t */
        == NULL)
        goto end_of_job;        /* quit if failed */
/* --- initialize ellipse subraster_t parameters --- */
    circsp->type = IMAGERASTER; /* image */
    circsp->symdef = NULL;      /* not applicable for image */
    circsp->baseline = height / 2 + 2;  /* is a little above center good? */
    circsp->size = size;        /* size (probably unneeded) */
/* -------------------------------------------------------------------------
draw the ellipse
-------------------------------------------------------------------------- */
    if (qptr != NULL) {          /* have quads */
        circle_raster(circsp->image,    /* embedded raster image */
                      0, 0,     /* row0,col0 are upper-left corner */
                      height - 1, width - 1,    /* row1,col1 are lower-right */
                      thickness,        /* line thickness is 1 pixel */
                      qptr);    /* "1234" quadrants to be drawn */
    } else {                        /* have theta0,theta1 */
        circle_recurse(circsp->image,   /* embedded raster image */
                       0, 0,    /* row0,col0 are upper-left corner */
                       height - 1, width - 1,   /* row1,col1 are lower-right */
                       thickness,       /* line thickness is 1 pixel */
                       theta0, theta1); /* theta0,theta1 arc to be drawn */
    }

end_of_job:
    if (workingparam != NULL)
        *workingparam = origin;
    return circsp;
}

/* ==========================================================================
 * Function:    rastbezier ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \bezier handler, returns subraster_t corresponding to bezier
 *      parameters (col0,row0)(col1,row1)(colt,rowt)
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \bezier to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \bezier
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to bezier
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \bezier(col1,row1)(colt,rowt)
 *        o col0=0,row0=0 assumed, i.e., given by
 *      \picture(){~(col0,row0){\bezier(col1,row1)(colt,rowt)}~}
 * ======================================================================= */
static subraster_t *rastbezier(const char **expression, int size, const subraster_t *basesp,
                               int arg1, int arg2, int arg3)
{
    subraster_t *bezsp = NULL;    /* subraster_t for bezier */
    char bezexpr[129], *xptr = bezexpr; /*\bezier(r,c)(r,c)(r,c) */
    double r0 = 0.0, c0 = 0.0, r1 = 0.0, c1 = 0.0, rt = 0.0, ct = 0.0;  /* bezier points */
    double rmid = 0.0, cmid = 0.0; /* coords at parameterized midpoint */
    double rmin = 0.0, cmin = 0.0; /* minimum r,c */
    double rmax = 0.0, cmax = 0.0; /* maximum r,c */
    double rdelta = 0.0, cdelta = 0.0;     /* rmax-rmin, cmax-cmin */
    double r = 0.0, c = 0.0;       /* some point */
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
#if DEBUG_LOG_LEVEL >= 29
    fprintf(msgfp,
            "rastbezier> width,height,origin=%d,%d,%d; c0,r0=%g,%g; "
            "c1,r1=%g,%g\n rmin,mid,max=%g,%g,%g; cmin,mid,max=%g,%g,%g\n",
            width, height, origin, c0, r0, c1, r1, rmin, rmid, rmax,
            cmin, cmid, cmax);
#endif
/* --- sanity check on width,height args --- */
    if (width < 1 || width > 600 || height < 1 || height > 600) {
        goto end_of_job;
    }

/* --- allocate and initialize subraster_t for constructed bezier --- */
    bezsp = new_subraster(width, height, pixsz);
    if (bezsp == NULL) {
        goto end_of_job;
    }

    bezsp->type = IMAGERASTER;  /* image */
    bezsp->symdef = NULL;       /* not applicable for image */
    bezsp->baseline = height / 2 + 2;   /* is a little above center good? */
    bezsp->size = size;         /* size (probably unneeded) */

    bezier_raster(bezsp->image, /* embedded raster image */
                  r0, c0,       /* row0,col0 are lower-left corner */
                  r1, c1,       /* row1,col1 are upper-right */
                  rt, ct);      /* bezier tangent point */
end_of_job:
    if (workingparam != NULL)
        *workingparam = origin;
    return bezsp;
}

/* ==========================================================================
 * Function:    rastraise ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \raisebox{lift}{subexpression} handler, returns subraster_t
 *      containing subexpression with its baseline "lifted" by lift
 *      pixels, scaled by \unitlength, or "lowered" if lift arg
 *      negative
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \raisebox to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \raisebox
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to \raisebox
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \raisebox{lift}{subexpression}
 *        o
 * ======================================================================= */
static subraster_t *rastraise(const char **expression, int size, const subraster_t *basesp,
                              int arg1, int arg2, int arg3)
{
    char subexpr[MAXSUBXSZ + 1], *liftexpr = subexpr;   /* args */
    subraster_t *raisesp = NULL;  /* rasterize subexpr to be raised */
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
    raisesp = rasterize(subexpr, size);
    if (raisesp == NULL) {
        goto end_of_job;
    }

/* --- raise/lower baseline --- */
    raisesp->baseline += lift; /* new baseline (no height checks) */
    rastlift = lift;           /* set global to signal adjustment */

end_of_job:
    return raisesp;
}

/* ==========================================================================
 * Function:    rastmagnify ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \magnify{magstep}{subexpression} handler, returns subraster_t
 *      containing magnified subexpression
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \reflectbox to
 *              be rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \reflectbox
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to \magnify
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \magnify{magstep}{subexpression}
 *        o
 * ======================================================================= */
static subraster_t *rastmagnify(const char **expression, int size, const subraster_t *basesp,
                                int arg1, int arg2, int arg3)
{
    char subexpr[MAXSUBXSZ + 1], *magexpr = subexpr;    /* args */
    subraster_t *magsp = NULL;    /* subraster_t for magnified subexpr */
    raster_t *magrp = NULL;       /* magnify subraster_t->image */
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
end_of_job:
    return magsp;
}

/* ==========================================================================
 * Function:    rastreflect ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \reflectbox[axis]{subexpression} handler, returns subraster_t
 *      containing subexpression reflected horizontally (i.e., around
 *      vertical axis, |_ becomes _|) if [axis] not given or axis=1,
 *      or reflected vertically if axis=2 given.
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \reflectbox to
 *              be rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \reflectbox
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to \reflectbox
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \reflectbox[axis]{subexpression}
 *        o
 * ======================================================================= */
static subraster_t *rastreflect(const char **expression, int size, const subraster_t *basesp,
                                int arg1, int arg2, int arg3)
{
    char subexpr[MAXSUBXSZ + 1], *axisexpr = subexpr;   /* args */
    subraster_t *refsp = NULL;    /* subraster_t for reflected subexpr */
    raster_t *refrp = NULL;       /* reflect subraster_t->image */
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
end_of_job:
    return refsp;
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
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \eval
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) subraster_t ptr to date stamp
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rasteval(const char **expression, int size, const subraster_t *basesp,
                             int arg1, int arg2, int arg3)
{
    char subexpr[MAXSUBXSZ];    /* arg to be evaluated */
    subraster_t *evalsp = NULL;   /* rasterize evaluated expression */
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

end_of_job:
    return evalsp;
}

/* ==========================================================================
 * Function:    rastfbox ( expression, size, basesp, arg1, arg2, arg3 )
 * Purpose: \fbox{subexpression} handler, returns subraster_t
 *      containing subexpression with frame box drawn around it
 * --------------------------------------------------------------------------
 * Arguments:   expression (I/O) char **  to first char of null-terminated
 *              string immediately following \fbox to be
 *              rasterized, and returning ptr immediately
 *              following last character processed.
 *      size (I)    int containing 0-7 default font size
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \fbox
 *              (unused, but passed for consistency)
 *      arg1 (I)    int unused
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) ptr to subraster_t corresponding to \fbox
 *              requested, or NULL for any parsing error
 * --------------------------------------------------------------------------
 * Notes:     o Summary of syntax...
 *        \fbox[width][height]{subexpression}
 *        o
 * ======================================================================= */
static subraster_t *rastfbox(const char **expression, int size, const subraster_t *basesp,
                             int arg1, int arg2, int arg3)
{
    char subexpr[MAXSUBXSZ + 1], widtharg[512]; /* args */
    subraster_t *framesp = NULL;  /* rasterize subexpr to be framed */
    raster_t *bp = NULL;          /* framed image raster */
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
    if (width > 0 || fsides > 0) {      /* found leading [width], so... */
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
    }

    /* -------------------------------------------------------------------------
       obtain {subexpr} argument
       -------------------------------------------------------------------------- */
    /* --- parse for {subexpr} arg, and bump expression past it --- */
    *expression = texsubexpr(*expression, subexpr, 0, "{", "}", 0, 0);
/* --- rasterize subexpression to be framed --- */
    if (width < 0 || height < 0) {      /* no explicit dimensions given */
        framesp = rasterize(subexpr, size);
        if (framesp == NULL)
            goto end_of_job;
    } else {
        char composexpr[8192];  /* compose subexpr with empty box */
        sprintf(composexpr, "\\compose{\\hspace{%d}\\vspace{%d}}{%.8000s}",
                width, height, subexpr);
        framesp = rasterize(composexpr, size);
        if (framesp == NULL)
            goto end_of_job;
    }

/* --- draw border --- */
    if (fsides > 0)
        fthick += (100 * fsides);       /* embed fsides in fthick arg */
    bp = border_raster(framesp->image, -fwidth, -fwidth, fthick, 1);
    if (bp == NULL)
        goto end_of_job;        /* draw border and quit if failed */
/* --- replace original image and raise baseline to accommodate frame --- */
    framesp->image = bp;        /* replace image with framed one */
    if (!iscompose) {           /* simple border around subexpr */
        framesp->baseline += fwidth;    /* so just raise baseline */
    } else {
        framesp->baseline = (framesp->image)->height - 1;       /* set at bottom */
    }
end_of_job:
    return framesp;
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
 *      basesp (I)  subraster_t *  to character (or subexpression)
 *              immediately preceding \escape
 *              (unused, but passed for consistency)
 *      nargs (I)   int containing number of {}-args after
 *              \escape to be flushed along with it
 *      arg2 (I)    int unused
 *      arg3 (I)    int unused
 * --------------------------------------------------------------------------
 * Returns: ( subraster_t * ) NULL subraster_t ptr
 * --------------------------------------------------------------------------
 * Notes:     o
 * ======================================================================= */
static subraster_t *rastnoop(const char **expression, int size, const subraster_t *basesp,
                             int nargs, int arg2, int arg3)
{
    char subexpr[MAXSUBXSZ + 1];        /*dummy args eaten by \escape */
    subraster_t *noopsp = NULL;   /* rasterize subexpr */

/* --- flush accompanying args if necessary --- */
    if (nargs != NOVALUE && nargs > 0) {
        while (--nargs >= 0) {
            *expression = texsubexpr(*expression, subexpr, 0, "{", "}", 0, 0);  /*flush arg */
        }
    }
    return noopsp;
}








/* ---
 * mathchardefs for symbols recognized by mimetex
 * ---------------------------------------------- */
const mathchardef_t symtable[] =
{
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
    //{"\\ascii", ISSTRING, 1, NOVALUE, rastflags},
    //{"\\image", ISSTRING, 0, NOVALUE, rastflags},
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
