/**
 * @file aconf.h
 *
 * @copyright 2002-2015 Glyph & Cog, LLC
 */

#ifndef ACONF_H
#define ACONF_H

#include <aconf2.h>

/*
 * Use A4 paper size instead of Letter for PostScript output.
 */
#undef A4_PAPER

/*
 * Do not allow text selection.
 */
#undef NO_TEXT_SELECT

/*
 * Include support for OPI comments.
 */
#undef OPI_SUPPORT

/**
 * Enable multithreading support.
 */
#define MULTITHREADED   1

/*
 * Enable C++ exceptions.
 */
#undef USE_EXCEPTIONS

/*
 * Use fixed point (instead of floating point) arithmetic.
 */
#undef USE_FIXEDPOINT

/*
 * Enable support for CMYK output.
 */
#undef SPLASH_CMYK

/*
 * Enable support for DeviceN output.
 */
#undef SPLASH_DEVICEN

/*
 * Enable support for highlighted regions.
 */
#undef HIGHLIGHTED_REGIONS

/*
 * Full path for the system-wide xpdfrc file.
 */
#undef SYSTEM_XPDFRC

/*
 * Various include files and functions.
 */
#undef HAVE_MKSTEMP
#undef HAVE_MKSTEMPS
#undef HAVE_POPEN
#undef HAVE_STD_SORT
#undef HAVE_FSEEKO
#undef HAVE_FSEEK64
#define HAVE_FSEEKI64           1   /**< use _fseeki64, _ftelli64 functions */
#define _FILE_OFFSET_BITS       64  /**< not used */
#define _LARGE_FILES            1   /**< not used */
#define _LARGEFILE_SOURCE       1   /**< not used */

/*
 * This is defined if using FreeType 2.
 */
#undef HAVE_FREETYPE_H

/*
 * This is defined if using D-Type 4.
 */
#undef HAVE_DTYPE4_H

/*
 * This is defined if using libpaper.
 */
#undef HAVE_PAPER_H

/*
 * Defined if the Splash library is avaiable.
 */
#undef HAVE_SPLASH

/*
 * Defined if using lcms2.
 */
#undef HAVE_LCMS

/*
 * Defined for evaluation mode.
 */
#undef EVAL_MODE

/*
 * Defined when building the closed source XpdfReader binary.
 */
#undef BUILDING_XPDFREADER

#define NO_DCT_STREAM       /**< don't extract JPG images */
#define NO_CCITT_STREAM     /**< don't extract FAX images */
#define NO_JBIG_STREAM      /**< don't extract B/W images */
#define NO_JPX_STREAM       /**< don't extract JPG2000 images */
#define NO_BARCODE          /**< don't create/parse bacodes */
#define NO_EMBEDDED_CONTENT /**< don't extract embedded files */
#define DISABLE_OUTLINE     /**< don't extract outline structures */

#endif
