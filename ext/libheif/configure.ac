AC_PREREQ([2.68])
AC_INIT([libheif], [1.12.0], [opensource@struktur.de])
AC_CONFIG_SRCDIR([libheif/box.cc])
AC_CONFIG_HEADERS([config.h])

# Note: do not forget to set the version in the CMakeLists.txt file accordingly
PROJECT_VERSION_MAJOR=1
PROJECT_VERSION_MINOR=12
PROJECT_VERSION_PATCH=0
PROJECT_VERSION_TWEAK=0
AC_SUBST(PROJECT_VERSION_MAJOR)
AC_SUBST(PROJECT_VERSION_MINOR)
AC_SUBST(PROJECT_VERSION_PATCH)
AC_SUBST(PROJECT_VERSION_TWEAK)

# From https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html:
# If the library source code has changed at all since the last update, then increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
# If any interfaces have been added, removed, or changed since the last update, increment current, and set revision to 0.
# If any interfaces have been added since the last public release, then increment age.
# If any interfaces have been removed or changed since the last public release, then set age to 0.

LIBHEIF_CURRENT=13
LIBHEIF_REVISION=0
LIBHEIF_AGE=12
AC_SUBST(LIBHEIF_CURRENT)
AC_SUBST(LIBHEIF_REVISION)
AC_SUBST(LIBHEIF_AGE)

AC_CANONICAL_SYSTEM
LT_INIT
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([.])

# Checks for programs.
AM_PROG_AS
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_GREP
AC_ARG_ENABLE([go], AS_HELP_STRING([--disable-go],
    [Disable building Go code / examples.]))
if eval "test x$enable_go = x" ; then
    AC_PATH_PROG([GO], [go], [no], [$PATH$PATH_SEPARATOR/usr/lib/go-1.10$PATH_SEPARATOR/usr/lib/go-1.9$PATH_SEPARATOR/usr/lib/go-1.8$PATH_SEPARATOR/usr/lib/go-1.7$PATH_SEPARATOR/usr/lib/go-1.6])
    AC_SUBST(GO)
fi
AM_CONDITIONAL([HAVE_GO], [test "x$GO" != "xno" && test "x$GO" != "x"])
AC_SUBST(HAVE_GO)

AC_ARG_ENABLE([examples], AS_HELP_STRING([--disable-examples],
    [Disable building examples.]), [], [enable_examples=yes])
AM_CONDITIONAL([WITH_EXAMPLES], [test "x$enable_examples" = "xyes"])
AC_SUBST(WITH_EXAMPLES)

AM_CONDITIONAL(MINGW, expr $host : '.*-mingw' >/dev/null 2>&1)

AM_INIT_AUTOMAKE([1.13 foreign])
AM_EXTRA_RECURSIVE_TARGETS([format test])

AX_CXX_COMPILE_STDCXX_11()

AC_HEADER_STDBOOL
AC_CHECK_HEADERS([inttypes.h stddef.h unistd.h])
AC_C_INLINE
AC_FUNC_ERROR_AT_LINE

AC_TYPE_SIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

AC_CHECK_LIB([pthread], [pthread_create])

AC_ARG_ENABLE([visibility], AS_HELP_STRING([--disable-visibility],
    [Disable visibility definitions.]),
    [HAVE_VISIBILITY=0],
    [gl_VISIBILITY])

AC_ARG_ENABLE([tests], AS_HELP_STRING([--enable-tests],
    [Enable compiling tests ('visibility' will be disabled).]))
AS_IF([test "x$enable_tests" = "xyes"], [
    HAVE_TESTS=1
    enable_visibility=no
    HAVE_VISIBILITY=0
    CFLAG_VISIBILITY=
])
AM_CONDITIONAL([HAVE_TESTS], [test "x$HAVE_TESTS" == "x1"])

AM_CONDITIONAL([HAVE_VISIBILITY], [test "x$HAVE_VISIBILITY" != "x0"])
if eval "test x$enable_visibility = x" ; then enable_visibility=yes ; fi

REQUIRES_PRIVATE=""
AC_SUBST([REQUIRES_PRIVATE])

PKG_PROG_PKG_CONFIG

have_avif_decoder="no"
have_avif_encoder="no"

AC_ARG_ENABLE([aom], AS_HELP_STRING([--disable-aom],
    [Disable building of aom decoder/encoder.]))
if eval "test x$enable_aom = x" ; then enable_aom=yes ; fi
if eval "test x$enable_aom != xno"; then
    PKG_CHECK_MODULES([aom], [aom], [
        AC_DEFINE([HAVE_AOM], [1], [Whether aom was found.])
        AC_SUBST(aom_CFLAGS)
        AC_SUBST(aom_LIBS)
        REQUIRES_PRIVATE="$REQUIRES_PRIVATE aom"
        have_aom="yes"
    ], [have_aom="no"])

    if eval "test x$have_aom = xyes"; then
        AC_CHECK_HEADER([aom/aom_decoder.h], [
            AC_DEFINE([HAVE_AOM_DECODER], [1], [Whether aom decoder was found.])
            have_aom_decoder="yes"
            have_avif_decoder="yes"
        ], [have_aom_decoder="no"])

        AC_CHECK_HEADER([aom/aom_encoder.h], [
            AC_DEFINE([HAVE_AOM_ENCODER], [1], [Whether aom encoder was found.])
            have_aom_encoder="yes"
            have_avif_encoder="yes"
        ], [have_aom_encoder="no"])
    fi
else
    have_aom="no"
    have_aom_decoder="no"
    have_aom_encoder="no"
fi
AM_CONDITIONAL([HAVE_AOM], [test "x$have_aom" = "xyes"])
AC_SUBST([have_aom])
AM_CONDITIONAL([HAVE_AOM_DECODER], [test "x$have_aom_decoder" = "xyes"])
AC_SUBST([have_aom_decoder])
AM_CONDITIONAL([HAVE_AOM_ENCODER], [test "x$have_aom_encoder" = "xyes"])
AC_SUBST([have_aom_encoder])

AC_ARG_ENABLE([libde265], AS_HELP_STRING([--disable-libde265],
    [Disable building of libde265 decoder.]))
if eval "test x$enable_libde265 = x" ; then enable_libde265=yes ; fi
if eval "test x$enable_libde265 != xno"; then
    PKG_CHECK_MODULES([libde265], [libde265], [
        AC_DEFINE([HAVE_LIBDE265], [1], [Whether libde265 was found.])
        AC_SUBST(libde265_CFLAGS)
        AC_SUBST(libde265_LIBS)
        REQUIRES_PRIVATE="$REQUIRES_PRIVATE libde265"
        have_libde265="yes"
    ], [have_libde265="no"])
else
    have_libde265="no"
fi
AM_CONDITIONAL([HAVE_LIBDE265], [test "x$have_libde265" = "xyes"])
AC_SUBST([have_libde265])

AC_ARG_ENABLE([x265], AS_HELP_STRING([--disable-x265],
    [Disable building of x265 encoder.]))
if eval "test x$enable_x265 = x" ; then enable_x265=yes ; fi
if eval "test x$enable_x265 != xno"; then
    PKG_CHECK_MODULES([x265], [x265], [
        AC_DEFINE([HAVE_X265], [1], [Whether x265 was found.])
        AC_SUBST(x265_CFLAGS)
        AC_SUBST(x265_LIBS)
        REQUIRES_PRIVATE="$REQUIRES_PRIVATE x265"
        have_x265="yes"
    ], [have_x265="no"])
else
    have_x265="no"
fi
AM_CONDITIONAL([HAVE_X265], [test "x$have_x265" = "xyes"])
AC_SUBST([have_x265])

AC_CHECK_HEADERS([jpeglib.h])
AC_CHECK_LIB([jpeg], [jpeg_CreateCompress], [
    AC_DEFINE([HAVE_LIBJPEG], [1], [Whether libjpeg was found.])
    libjpeg_CFLAGS=""
    AC_SUBST(libjpeg_CFLAGS)
    libjpeg_LIBS="-ljpeg"
    AC_SUBST(libjpeg_LIBS)
    have_libjpeg="yes"
], [have_libjpeg="no"])
if eval "test x$have_libjpeg = xno"; then
AC_CHECK_LIB([jpeg], [jpeg_destroy_compress], [
    AC_DEFINE([HAVE_LIBJPEG], [1], [Whether libjpeg was found.])
    libjpeg_CFLAGS=""
    AC_SUBST(libjpeg_CFLAGS)
    libjpeg_LIBS="-ljpeg"
    AC_SUBST(libjpeg_LIBS)
    have_libjpeg="yes"
], [have_libjpeg="no"])
fi
AM_CONDITIONAL([HAVE_LIBJPEG], [test "x$have_libjpeg" = "xyes"])
if eval "test x$have_libjpeg = xyes"; then
AC_MSG_CHECKING([for jpeg_write_icc_profile])
AC_LANG_PUSH(C++)
AC_TRY_COMPILE([
    #include <stddef.h>
    #include <stdio.h>
    #include <jpeglib.h>
],[
    jpeg_write_icc_profile(NULL, NULL, 0);
],[has_jpeg_write_icc_profile=yes],[has_jpeg_write_icc_profile=no]);
AC_LANG_POP(C++)
AC_MSG_RESULT([$has_jpeg_write_icc_profile])
if eval "test x$has_jpeg_write_icc_profile = xyes"; then
    AC_DEFINE(HAVE_JPEG_WRITE_ICC_PROFILE, 1, [Define to 1 if jpeg_write_icc_profile is available in libjpeg.])
fi
fi

PKG_CHECK_MODULES([libpng], [libpng], [
    AC_DEFINE([HAVE_LIBPNG], [1], [Whether libpng was found.])
    AC_SUBST(libpng_CFLAGS)
    AC_SUBST(libpng_LIBS)
    have_libpng="yes"
], [have_libpng="no"])
AM_CONDITIONAL([HAVE_LIBPNG], [test "x$have_libpng" = "xyes"])

AC_ARG_ENABLE([gdk-pixbuf], AS_HELP_STRING([--disable-gdk-pixbuf],
    [Disable building of gdk-pixbuf plugin.]))
if eval "test x$enable_gdk_pixbuf = x" ; then enable_gdk_pixbuf=yes ; fi
if eval "test x$enable_gdk_pixbuf != xno"; then
    PKG_CHECK_MODULES([gdkpixbuf], [gdk-pixbuf-2.0], [
        AC_DEFINE([HAVE_GDKPIXBUF2], [1], [Whether gdk-pixbuf-2.0 was found.])
        AC_SUBST(gdkpixbuf_CFLAGS)
        AC_SUBST(gdkpixbuf_LIBS)
        have_gdkpixbuf2="yes"
    ], [have_gdkpixbuf2="no"])
else
    have_gdkpixbuf2="no"
fi
AM_CONDITIONAL([HAVE_GDKPIXBUF2], [test "x$have_gdkpixbuf2" = "xyes"])

if eval "test x$have_gdkpixbuf2 = xyes"; then
    gdk_pixbuf_binary_version="`$PKG_CONFIG --variable=gdk_pixbuf_binary_version gdk-pixbuf-2.0`"
    gdk_pixbuf_binarydir="`$PKG_CONFIG --variable=gdk_pixbuf_binarydir gdk-pixbuf-2.0`"
    gdk_pixbuf_moduledir=`$PKG_CONFIG --variable gdk_pixbuf_moduledir gdk-pixbuf-2.0`
    gdk_pixbuf_cache_file=`$PKG_CONFIG --variable gdk_pixbuf_cache_file gdk-pixbuf-2.0`

    AC_SUBST([gdk_pixbuf_binary_version])
    AC_SUBST([gdk_pixbuf_binarydir])
    AC_SUBST([gdk_pixbuf_moduledir])
    AC_SUBST([gdk_pixbuf_cache_file])
fi

AC_ARG_ENABLE([rav1e], AS_HELP_STRING([--disable-rav1e],
    [Disable building of rav1e encoder.]))
if eval "test x$enable_rav1e = x" ; then enable_rav1e=yes ; fi
if eval "test x$enable_rav1e != xno"; then
    PKG_CHECK_MODULES([rav1e], [rav1e], [
        AC_DEFINE([HAVE_RAV1E], [1], [Whether rav1e was found.])
        AC_SUBST(rav1e_CFLAGS)
        AC_SUBST(rav1e_LIBS)
        have_avif_encoder="yes"
        REQUIRES_PRIVATE="$REQUIRES_PRIVATE rav1e"
        have_rav1e="yes"
    ], [have_rav1e="no"])
else
    have_rav1e="no"
fi
AM_CONDITIONAL([HAVE_RAV1E], [test "x$have_rav1e" = "xyes"])
AC_SUBST([have_rav1e])

PKG_CHECK_MODULES([dav1d], [dav1d], [
    AC_DEFINE([HAVE_DAV1D], [1], [Whether dav1d was found.])
    AC_SUBST(dav1d_CFLAGS)
    AC_SUBST(dav1d_LIBS)
    have_avif_decoder="yes"
    REQUIRES_PRIVATE="$REQUIRES_PRIVATE dav1d"
    have_dav1d="yes"
], [have_dav1d="no"])
AM_CONDITIONAL([HAVE_DAV1D], [test "x$have_dav1d" = "xyes"])

AC_SUBST(have_avif_decoder)
AC_SUBST(have_avif_encoder)

AC_ARG_ENABLE([libfuzzer], AS_HELP_STRING([--enable-libfuzzer],
    [Enable fuzzing with libFuzzer.]), [], [enable_libfuzzer=no])
if eval "test x$enable_libfuzzer != xno"; then
    AC_MSG_NOTICE([Enable libFuzzer])
    AC_DEFINE([HAVE_LIBFUZZER], [1], [Whether building with libFuzzer.])

    FUZZING_ENGINE="-lFuzzingEngine"
    if eval "test x$enable_libfuzzer != xyes"; then
        FUZZING_ENGINE="$enable_libfuzzer"
    fi
    AC_MSG_NOTICE([Fuzzing engine flags: $FUZZING_ENGINE])
    AC_SUBST([FUZZING_ENGINE])
fi
AM_CONDITIONAL([ENABLE_LIBFUZZER], [test "x$enable_libfuzzer" != "xno"])
AC_SUBST(ENABLE_LIBFUZZER)

AC_ARG_ENABLE([multithreading], AS_HELP_STRING([--disable-multithreading],
    [Disable multithreaded decoding.]))
if eval "test x$enable_multithreading = x" ; then enable_multithreading=yes ; fi
if eval "test x$enable_multithreading != xno"; then
    AC_MSG_NOTICE([Enable multithreading])
    AC_DEFINE([ENABLE_PARALLEL_TILE_DECODING], [1], [Whether we enable multithreaded decoding.])
fi
AC_SUBST(ENABLE_PARALLEL_TILE_DECODING)

WARNING_FLAGS="-Wall -Werror -Wsign-compare -Wconversion -Wno-sign-conversion -Wno-error=conversion -Wno-error=unused-parameter -Wno-error=deprecated-declarations"

CXXFLAGS="$CXXFLAGS $WARNING_FLAGS"
CFLAGS="$CFLAGS $WARNING_FLAGS"

AC_C_CXX_COMPILE_FLAGS([-Wno-error=potentially-evaluated-expression])
AC_C_CXX_COMPILE_FLAGS([-Wno-error=tautological-compare])
AC_C_CXX_COMPILE_FLAGS([-Wno-error=tautological-constant-out-of-range-compare])

AC_MSG_NOTICE([---------------------------------------])
AC_MSG_NOTICE([Multithreading: $enable_multithreading])
AC_MSG_NOTICE([Symbol visibility: $enable_visibility])
AC_MSG_NOTICE([libaom decoder: $have_aom_decoder])
AC_MSG_NOTICE([libaom encoder: $have_aom_encoder])
AC_MSG_NOTICE([rav1e encoder: $have_rav1e])
AC_MSG_NOTICE([dav1d decoder: $have_dav1d])
AC_MSG_NOTICE([libde265 decoder: $have_libde265])
AC_MSG_NOTICE([libx265 encoder: $have_x265])
AC_MSG_NOTICE([JPEG output: $have_libjpeg])
AC_MSG_NOTICE([PNG output: $have_libpng])
AC_MSG_NOTICE([GdkPixbuf2 loader: $have_gdkpixbuf2])
AC_MSG_NOTICE([Examples: $enable_examples])
AC_MSG_NOTICE([Tests: $enable_tests (tests will cause 'visibility' to be turned off)])
AC_MSG_NOTICE([---------------------------------------])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([examples/Makefile])
AC_CONFIG_FILES([extra/Makefile])
AC_CONFIG_FILES([fuzzing/Makefile])
AC_CONFIG_FILES([gdk-pixbuf/Makefile])
AC_CONFIG_FILES([go/Makefile])
AC_CONFIG_FILES([gnome/Makefile])
AC_CONFIG_FILES([scripts/Makefile])
AC_CONFIG_FILES([tests/Makefile])
AC_CONFIG_FILES([libheif/Makefile])
AC_CONFIG_FILES([libheif/heif_version.h])
AC_CONFIG_FILES([libheif.pc])
AC_OUTPUT
