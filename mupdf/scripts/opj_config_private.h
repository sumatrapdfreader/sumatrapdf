/* If you want to build the library manually without using
 * 'configure' or 'CMake'
 * then copy this file
 * 'opj_config_private.h.cmake.in'
 *	to
 * 'opj_config_private.h'
 *
 * Open 'opj_config_private.h' and change the file contents
 * if you want to define something because you know you have
 * BOTH installed the library AND the header file(s).
 * Then e.g. write
#define HAVE_LIBPNG 1
 *
 *
 * The file 'opj_config_private.h' will be included in some source files.
 * ==== YOU CAN NOT COMPILE WITHOUT IT. ====
 * === DO NOT FORGET TO CHANGE 'config.nix' APPROPRIATELY. ====
*/

#ifndef _WIN32
#define OPJ_HAVE_INTTYPES_H 1
#else
#undef OPJ_HAVE_INTTYPES_H
#endif

#define USE_JPIP
#define OPJ_PACKAGE_VERSION "2.0.0"

/* DO NOT DEFINE BOTH VERSIONS OF LCMS */
/* define to 1 if you have both liblcms and lcms.h installed */
#undef OPJ_HAVE_LIBLCMS1
/* #define OPJ_HAVE_LIBLCMS1 1 */

/* define to 1 if you have both liblcms2 and lcms2.h installed */
#undef OPJ_HAVE_LIBLCMS2
/* #define OPJ_HAVE_LIBLCMS2 1 */

/* define to 1 if you have both libpng and png.h installed */
#undef OPJ_HAVE_LIBPNG
/* #define OPJ_HAVE_LIBPNG 1 */

/* define to 1 if you have both libtiff and tiff.h installed */
#undef OPJ_HAVE_LIBTIFF
/* #define OPJ_HAVE_LIBTIFF 1 */

/*---------------- DO NOT CHANGE BELOW THIS LINE ----------------*/
#define PACKAGE_URL "http://www.openjpeg.org/"
#define PACKAGE_BUGREPORT "http://code.google.com/p/openjpeg/"
