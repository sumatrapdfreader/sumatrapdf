#
# LittleCMS 2 configure script
#

AC_PREREQ(2.60)

#
# Set the package name and version
#
AC_INIT(lcms2mt,2.9)

# Specify directory where m4 macros may be found.
AC_CONFIG_MACRO_DIR([m4])

#
# Libtool library revision control info
# See the libtool documentation under the heading "Libtool's versioning
# system" in order to understand the meaning of these fields
#
# Here are a set of rules to help you update your library version
# information:
#
#  1. Start with version information of `0:0:0' for each libtool library.
#  2. Update the version information only immediately before a public
#     release of your software. More frequent updates are unnecessary, and
#     only guarantee that the current interface number gets larger faster.
#  3. If the library source code has changed at all since the last update,
#     then increment revision (`c:r:a' becomes `c:r+1:a').
#  4. If any interfaces have been added, removed, or changed since the last
#     update, increment current, and set revision to 0.
#  5. If any interfaces have been added since the last public release, then
#     increment age.
#  6. If any interfaces have been removed since the last public release,
#     then set age to 0.
#
LIBRARY_CURRENT=2
LIBRARY_REVISION=8
LIBRARY_AGE=0

AC_SUBST(LIBRARY_CURRENT)dnl
AC_SUBST(LIBRARY_REVISION)dnl
AC_SUBST(LIBRARY_AGE)dnl

# Obtain system type by running config.guess
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([foreign 1.7.2 no-define dist-zip subdir-objects])


# Check for programs
AC_PROG_CC_STDC
AC_PROG_CPP
AC_PROG_CXX

#AM_PROG_LD
#AC_SUBST(LD)
#AC_PROG_INSTALL
#AC_PROG_MAKE_SET
#AC_PROG_LN_S

#
# Tests for Windows
#
AC_EXEEXT
AC_OBJEXT

# Configure libtool
AC_ENABLE_SHARED
AC_ENABLE_STATIC
AC_LIBTOOL_WIN32_DLL
AC_LIBTOOL_SETUP
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

# Add configure option --enable-maintainer-mode which enables dependency
# checking and generation useful to package maintainers.  This is made an
# option to avoid confusing end users.
AM_MAINTAINER_MODE

# If the C compiler supports the keyword inline, do nothing. Otherwise
# define inline to __inline__ or __inline if it accepts one of those,
# otherwise define inline to be empty.
AC_C_INLINE
AC_SUBST(inline)

# Check if the C compiler supports the "visibility" function attribute
# If supported, defines HAVE_FUNC_ATTRIBUTE_VISIBILITY
AX_GCC_FUNC_ATTRIBUTE(visibility)

# Check if the compiler supports "-fvisibility=hidden" and if yes, add it to CFLAGS
# This means that symbols that are not marked explicitly for export (CMSAPI)
# will not be reachable in the shared library.
AX_APPEND_COMPILE_FLAGS(["-fvisibility=hidden"])

# If words are stored with the most significant byte first (like
# Motorola and SPARC CPUs), define `WORDS_BIGENDIAN'.
AC_C_BIGENDIAN

# Point to JPEG installed in DIR or disable JPEG with --without-jpeg.
AC_ARG_WITH(jpeg,
            [  --with-jpeg=DIR         use jpeg installed in DIR],
            [
            if [ test "x$withval" = "xno" ]; then
              [with_jpeg='no']
            else
              if [ test "x$withval" != "xyes" ]; then
                with_jpeg=$withval
                JPEG_DIR=$withval
                CPPFLAGS="$CPPFLAGS -I$JPEG_DIR/include"
                LDFLAGS="$LDFLAGS -L$JPEG_DIR/lib"
              fi
              [with_jpeg='yes']
            fi
            ],
	        [with_jpeg='yes'])

# Point to TIFF installed in DIR or disable TIFF with --without-tiff.
AC_ARG_WITH(tiff,
            [  --with-tiff=DIR         use tiff installed in DIR],
            [
            if [ test "x$withval" = "xno" ]; then
              [with_tiff='no']
            else
              if [ test "x$withval" != "xyes" ]; then
                with_tiff=$withval
                TIFF_DIR=$withval
                CPPFLAGS="$CPPFLAGS -I$TIFF_DIR/include"
                LDFLAGS="$LDFLAGS -L$TIFF_DIR/lib"
              fi
              [with_tiff='yes']
            fi
            ],
	        [with_tiff='yes'])

# Disable ZLIB
AC_ARG_WITH(zlib,
	      [  --without-zlib          disable ZLIB support],
	      [with_zlib=$withval],
	      [with_zlib='yes'])

#
# Determine POSIX threads settings
#
# Enable support for POSIX thread APIs
AC_ARG_WITH(threads,
	      AS_HELP_STRING([--without-threads],
                             [disable POSIX threads API support]),
	      [with_threads=$withval],
	      [with_threads='yes'])

have_threads=no
if test "$with_threads" != 'no'
then

  ACX_PTHREAD()

  if test "$acx_pthread_ok" = yes
  then
    have_threads=yes

    DEF_THREAD="$PTHREAD_CFLAGS"
    CFLAGS="$CFLAGS $DEF_THREAD"
    CXXFLAGS="$CXXFLAGS $DEF_THREAD"

    if test "$CC" != "$PTHREAD_CC"
    then
      AC_MSG_WARN([Replacing compiler $CC with compiler $PTHREAD_CC to support pthreads.])
      CC="$PTHREAD_CC"
    fi
    if test "$CXX" != "$PTHREAD_CXX"
    then
      AC_MSG_WARN([Replacing compiler $CXX with compiler $PTHREAD_CXX to support pthreads.])
      CXX="$PTHREAD_CXX"
    fi
  fi
fi


#
# Find math library
#
LIB_MATH=''
AC_CHECK_LIB(m,sqrt,LIB_MATH="-lm",,)
LIBS="$LIB_MATH $LIBS"
AC_SUBST(LIB_MATH)

#
# Find Posix threads library
#
LIB_THREAD=''
if test "$with_threads" != 'no' && test "$have_threads" = 'yes'
then
  for lib in pthread pthreads
  do
    if test "x$PTHREAD_LIBS" = "x" ; then
      AC_CHECK_LIB([$lib],pthread_mutex_lock,[PTHREAD_LIBS=-l$lib],,)
    fi
  done

  LIB_THREAD="$PTHREAD_LIBS"
  LIBS="$LIBS $LIB_THREAD"
  AC_DEFINE(HasTHREADS,1,[Define if you have pthreads library])
else
  AC_DEFINE(HasTHREADS,0,[Define if you don't have pthreads library])
fi
AC_SUBST(LIB_THREAD)

#
# Check for JPEG
#
have_jpeg='no'
LIB_JPEG=''
if test ! "$with_jpeg" = 'no'
then
    AC_MSG_CHECKING([for JPEG support])
    AC_MSG_RESULT()
    failed=0;
    passed=0;
    AC_CHECK_HEADER(jconfig.h,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
    AC_CHECK_HEADER(jerror.h,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
    AC_CHECK_HEADER(jmorecfg.h,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
    AC_CHECK_HEADER(jpeglib.h,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
    AC_CHECK_LIB(jpeg,jpeg_read_header,passed=`expr $passed + 1`,failed=`expr $failed + 1`,)

# Test for compatible JPEG library
if test ! "$ac_cv_jpeg_version_ok" = 'yes' ; then
AC_CACHE_CHECK(for JPEG library is version 6b or later, ac_cv_jpeg_version_ok,
[AC_TRY_COMPILE(
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
,
changequote(<<, >>)dnl
<<
#if JPEG_LIB_VERSION < 62
#error IJG JPEG library must be version 6b or newer!
#endif
return 0;
>>,
changequote([, ])dnl
ac_cv_jpeg_version_ok='yes',
ac_cv_jpeg_version_ok='no')])
if test "$ac_cv_jpeg_version_ok" = 'yes' ; then
 AC_MSG_RESULT(yes)
 passed=`expr $passed + 1`
else
 AC_MSG_RESULT(no)
 failed=`expr $failed + 1`
fi
fi
    AC_MSG_CHECKING(if JPEG package is complete)
    if test $passed -gt 0
    then
    if test $failed -gt 0
    then
	AC_MSG_RESULT(no -- some components failed test)
        have_jpeg='no (failed tests)'
    else
	LIB_JPEG='-ljpeg'
	LIBS="$LIB_JPEG $LIBS"
	AC_DEFINE(HasJPEG,1,Define if you have JPEG library)
	AC_MSG_RESULT(yes)
        have_jpeg='yes'
    fi
    else
        AC_MSG_RESULT(no)
    fi
fi
AM_CONDITIONAL(HasJPEG, test "$have_jpeg" = 'yes')
AC_SUBST(LIB_JPEG)

#
# Check for ZLIB
#
have_zlib='no'
dnl PNG requires zlib so enable zlib check if PNG is requested
if test ! "$with_zlib" = 'no' || test ! "$with_png" = 'no'
then
  LIB_ZLIB=''
  AC_MSG_CHECKING(for ZLIB support )
  AC_MSG_RESULT()
  failed=0;
  passed=0;
  AC_CHECK_HEADER(zconf.h,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
  AC_CHECK_HEADER(zlib.h,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
  AC_CHECK_LIB(z,compress,passed=`expr $passed + 1`,failed=`expr $failed + 1`,)
  AC_CHECK_LIB(z,uncompress,passed=`expr $passed + 1`,failed=`expr $failed + 1`,)
  AC_CHECK_LIB(z,deflate,passed=`expr $passed + 1`,failed=`expr $failed + 1`,)
  AC_CHECK_LIB(z,inflate,passed=`expr $passed + 1`,failed=`expr $failed + 1`,)
  AC_CHECK_LIB(z,gzseek,passed=`expr $passed + 1`,failed=`expr $failed + 1`,)
  AC_CHECK_LIB(z,gztell,passed=`expr $passed + 1`,failed=`expr $failed + 1`,)
  AC_MSG_CHECKING(if ZLIB package is complete)
  if test $passed -gt 0
  then
    if test $failed -gt 0
    then
      AC_MSG_RESULT(no -- some components failed test)
      have_zlib='no (failed tests)'
    else
      LIB_ZLIB='-lz'
      LIBS="$LIB_ZLIB $LIBS"
      AC_DEFINE(HasZLIB,1,Define if you have zlib compression library)
      AC_MSG_RESULT(yes)
      have_zlib='yes'
    fi
  else
    AC_MSG_RESULT(no)
  fi
fi
AM_CONDITIONAL(HasZLIB, test "$have_zlib" = 'yes')
AC_SUBST(LIB_ZLIB)

#
# Check for TIFF
#
have_tiff='no'
LIB_TIFF=''
if test ! "$with_tiff" = 'no'
then
    AC_MSG_CHECKING([for TIFF support])
    AC_MSG_RESULT()
    failed=0;
    passed=0;
    AC_CHECK_HEADER(tiff.h,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
    AC_CHECK_HEADER(tiffio.h,passed=`expr $passed + 1`,failed=`expr $failed + 1`)
    AC_CHECK_LIB(tiff,TIFFOpen,passed=`expr $passed + 1`,failed=`expr $failed + 1`,)
    AC_CHECK_LIB(tiff,TIFFClientOpen,passed=`expr $passed + 1`,failed=`expr $failed + 1`,)
    AC_CHECK_LIB(tiff,TIFFIsByteSwapped,passed=`expr $passed + 1`,failed=`expr $failed + 1`,)
    AC_MSG_CHECKING(if TIFF package is complete)
    if test $passed -gt 0
    then
    if test $failed -gt 0
    then
	AC_MSG_RESULT(no -- some components failed test)
	have_tiff='no (failed tests)'
    else
	LIB_TIFF='-ltiff'
	LIBS="$LIB_TIFF $LIBS"
	AC_DEFINE(HasTIFF,1,Define if you have TIFF library)
	AC_MSG_RESULT(yes)
	have_tiff='yes'
	AC_CHECK_HEADERS(tiffconf.h)
    fi
    else
    AC_MSG_RESULT(no)
    fi
fi
AM_CONDITIONAL(HasTIFF, test "$have_tiff" = 'yes')
AC_SUBST(LIB_TIFF)


# Libraries that the LCMS2MT library depends on
LCMS2MT_LIB_DEPLIBS="$LIB_MATH $LIB_THREAD"
LCMS2MT_LIB_DEPLIBS=`echo $LCMS2MT_LIB_DEPLIBS | sed -e 's/  */ /g'`
AC_SUBST(LCMS2MT_LIB_DEPLIBS)

# Libraries that the jpegicc program depends on
JPEGICC_DEPLIBS="$LIB_JPEG $LIB_MATH $LIB_THREAD"
JPEGICC_DEPLIBS=`echo $JPEGICC_DEPLIBS | sed -e 's/  */ /g'`
AC_SUBST(JPEGICC_DEPLIBS)

# Libraries that the tifficc program depends on
TIFFICC_DEPLIBS="$LIB_TIFF $LIB_JPEG $LIB_ZLIB $LIB_MATH $LIB_THREAD"
TIFFICC_DEPLIBS=`echo $TIFFICC_DEPLIBS | sed -e 's/  */ /g'`
AC_SUBST(TIFFICC_DEPLIBS)

LIBS=''

#
# Perform substitutions
#
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([lcms2mt.pc])
AC_CONFIG_FILES([include/Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([utils/tificc/Makefile])
AC_CONFIG_FILES([utils/transicc/Makefile])
AC_CONFIG_FILES([utils/linkicc/Makefile])
AC_CONFIG_FILES([utils/jpgicc/Makefile])
AC_CONFIG_FILES([utils/psicc/Makefile])
AC_CONFIG_FILES([testbed/Makefile])
AC_OUTPUT
