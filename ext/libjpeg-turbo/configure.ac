#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.56])
AC_INIT([libjpeg-turbo], [1.3.0])
BUILD=`date +%Y%m%d`

AM_INIT_AUTOMAKE([-Wall foreign dist-bzip2])
AC_PREFIX_DEFAULT(/opt/libjpeg-turbo)

# Always build with prototypes
AC_DEFINE([HAVE_PROTOTYPES], 1, [Define if your compiler supports prototypes])

# Checks for programs.
SAVED_CFLAGS=${CFLAGS}
SAVED_CPPFLAGS=${CPPFLAGS}
AC_PROG_CPP
AC_PROG_CC
AM_PROG_AS
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_LN_S

# When the prefix is /opt/libjpeg-turbo, we assume that an "official" binary is
# being created, and thus we install things into specific locations.

old_prefix=${prefix}
if test "x$prefix" = "xNONE" -a "x$ac_default_prefix" != "x"; then
  prefix=$ac_default_prefix
fi
DATADIR=`eval echo ${datadir}`
DATADIR=`eval echo $DATADIR`
if test "$DATADIR" = "/opt/libjpeg-turbo/share"; then
  datadir='${prefix}'
fi
DATADIR=`eval echo ${datarootdir}`
DATADIR=`eval echo $DATADIR`
if test "$DATADIR" = "/opt/libjpeg-turbo/share"; then
  datarootdir='${prefix}'
fi

old_exec_prefix=${exec_prefix}
if test "x$exec_prefix" = "xNONE"; then
  exec_prefix=${prefix}
fi

if test "x${libdir}" = 'x${exec_prefix}/lib' -o "x${libdir}" = 'x${prefix}/lib'; then
  LIBDIR=`eval echo ${libdir}`
  LIBDIR=`eval echo $LIBDIR`
  if test "$LIBDIR" = "/opt/libjpeg-turbo/lib"; then
    case $host_os in
      darwin*)
        ;;
      *)
        case "$host_cpu" in
          x86_64 | amd64)
            libdir='${exec_prefix}/lib64'
            ;;
          i*86 | x86 | ia32)
            libdir='${exec_prefix}/lib32'
            ;;
        esac
        ;;
    esac
  fi
fi
exec_prefix=${old_exec_prefix}
prefix=${old_prefix}

# Check whether compiler supports pointers to undefined structures
AC_MSG_CHECKING(whether compiler supports pointers to undefined structures)
AC_TRY_COMPILE([ typedef struct undefined_structure * undef_struct_ptr; ], ,
  AC_MSG_RESULT(yes),
  [AC_MSG_RESULT(no)
   AC_DEFINE([INCOMPLETE_TYPES_BROKEN], [1],
     [Compiler does not support pointers to undefined structures.])])

if test "x${GCC}" = "xyes"; then
  if test "x${SAVED_CFLAGS}" = "x"; then
    CFLAGS=-O3
  fi
  if test "x${SAVED_CPPFLAGS}" = "x"; then
    CPPFLAGS=-Wall
  fi
fi

AC_CHECK_DECL([__SUNPRO_C], [SUNCC="yes"], [SUNCC="no"])
if test "x${SUNCC}" = "xyes"; then
  if test "x${SAVED_CFLAGS}" = "x"; then
    CFLAGS=-xO5
  fi
fi

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stddef.h stdlib.h string.h])
AC_CHECK_HEADER([sys/types.h],
  AC_DEFINE([NEED_SYS_TYPES_H], 1, [Define if you have sys/types.h]))

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_CHAR_UNSIGNED
AC_C_INLINE
AC_TYPE_SIZE_T
AC_CHECK_TYPES([unsigned char, unsigned short])

AC_MSG_CHECKING([if right shift is signed])
AC_TRY_RUN(
  [#include <stdio.h>
   int is_shifting_signed (long arg) {
     long res = arg >> 4;

     if (res == -0x7F7E80CL)
       return 1; /* right shift is signed */

     /* see if unsigned-shift hack will fix it. */
     /* we can't just test exact value since it depends on width of long... */
     res |= (~0L) << (32-4);
     if (res == -0x7F7E80CL)
       return 0; /* right shift is unsigned */

     printf("Right shift isn't acting as I expect it to.\n");
     printf("I fear the JPEG software will not work at all.\n\n");
     return 0; /* try it with unsigned anyway */
   }
   int main (void) {
     exit(is_shifting_signed(-0x7F7E80B1L));
   }],
  [AC_MSG_RESULT(no)
   AC_DEFINE([RIGHT_SHIFT_IS_UNSIGNED], 1, [Define if shift is unsigned])],
  [AC_MSG_RESULT(yes)],
  [AC_MSG_RESULT(Assuming that right shift is signed on target machine.)])

# test whether global names are unique to at least 15 chars
AC_MSG_CHECKING([for short external names])
AC_TRY_LINK(
  [int possibly_duplicate_function () { return 0; }
   int possibly_dupli_function () { return 1; }], [ ],
  [AC_MSG_RESULT(ok)],
  [AC_MSG_RESULT(short)
   AC_DEFINE([NEED_SHORT_EXTERNAL_NAMES], 1,
     [Define if you need short function names])])

# Checks for library functions.
AC_CHECK_FUNCS([memset memcpy], [],
  [AC_DEFINE([NEED_BSD_STRINGS], 1,
     [Define if you have BSD-like bzero and bcopy])])

AC_MSG_CHECKING([libjpeg API version])
AC_ARG_VAR(JPEG_LIB_VERSION, [libjpeg API version (62, 70, or 80)])
if test "x$JPEG_LIB_VERSION" = "x"; then
  AC_ARG_WITH([jpeg7],
    AC_HELP_STRING([--with-jpeg7],
      [Emulate libjpeg v7 API/ABI (this makes libjpeg-turbo backward incompatible with libjpeg v6b.)]))
  AC_ARG_WITH([jpeg8],
    AC_HELP_STRING([--with-jpeg8],
      [Emulate libjpeg v8 API/ABI (this makes libjpeg-turbo backward incompatible with libjpeg v6b.)]))
  if test "x${with_jpeg8}" = "xyes"; then
    JPEG_LIB_VERSION=80
  else
    if test "x${with_jpeg7}" = "xyes"; then
      JPEG_LIB_VERSION=70
    else
      JPEG_LIB_VERSION=62
    fi
  fi
fi
JPEG_LIB_VERSION_DECIMAL=`expr $JPEG_LIB_VERSION / 10`.`expr $JPEG_LIB_VERSION % 10`
AC_SUBST(JPEG_LIB_VERSION_DECIMAL)
AC_MSG_RESULT([$JPEG_LIB_VERSION_DECIMAL])
AC_DEFINE_UNQUOTED(JPEG_LIB_VERSION, [$JPEG_LIB_VERSION],
  [libjpeg API version])

AC_ARG_VAR(SO_MAJOR_VERSION,
  [Major version of the libjpeg-turbo shared library (default is determined by the API version)])
AC_ARG_VAR(SO_MINOR_VERSION,
  [Minor version of the libjpeg-turbo shared library (default is determined by the API version)])
if test "x$SO_MAJOR_VERSION" = "x"; then
  case "$JPEG_LIB_VERSION" in
    62)  SO_MAJOR_VERSION=$JPEG_LIB_VERSION ;;
    *)   SO_MAJOR_VERSION=`expr $JPEG_LIB_VERSION / 10` ;;
  esac
fi
if test "x$SO_MINOR_VERSION" = "x"; then
  case "$JPEG_LIB_VERSION" in
    80)  SO_MINOR_VERSION=2 ;;
    *)   SO_MINOR_VERSION=0 ;;
  esac
fi

RPM_CONFIG_ARGS=

# Memory source/destination managers
SO_AGE=0
MEM_SRCDST_FUNCTIONS=
if test "x${with_jpeg8}" != "xyes"; then
  AC_MSG_CHECKING([whether to include in-memory source/destination managers])
  AC_ARG_WITH([mem-srcdst],
    AC_HELP_STRING([--without-mem-srcdst],
      [Do not include in-memory source/destination manager functions when emulating the libjpeg v6b or v7 API/ABI]))
  if test "x$with_mem_srcdst" != "xno"; then
    AC_MSG_RESULT(yes)
    AC_DEFINE([MEM_SRCDST_SUPPORTED], [1],
      [Support in-memory source/destination managers])
    SO_AGE=1
    MEM_SRCDST_FUNCTIONS="global:  jpeg_mem_dest;  jpeg_mem_src;";
  else
    AC_MSG_RESULT(no)
    RPM_CONFIG_ARGS="$RPM_CONFIG_ARGS --without-mem-srcdst"
  fi
fi

AC_MSG_CHECKING([libjpeg shared library version])
AC_MSG_RESULT([$SO_MAJOR_VERSION.$SO_AGE.$SO_MINOR_VERSION])
LIBTOOL_CURRENT=`expr $SO_MAJOR_VERSION + $SO_AGE`
AC_SUBST(LIBTOOL_CURRENT)
AC_SUBST(SO_MAJOR_VERSION)
AC_SUBST(SO_MINOR_VERSION)
AC_SUBST(SO_AGE)
AC_SUBST(MEM_SRCDST_FUNCTIONS)

AC_DEFINE_UNQUOTED(LIBJPEG_TURBO_VERSION, [$VERSION], [libjpeg-turbo version])

VERSION_SCRIPT=yes
AC_ARG_ENABLE([ld-version-script],
  AS_HELP_STRING([--disable-ld-version-script],
    [Disable linker version script for libjpeg-turbo (default is to use linker version script if the linker supports it)]),
  [VERSION_SCRIPT=$enableval], [])

AC_MSG_CHECKING([whether the linker supports version scripts])
SAVED_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS -Wl,--version-script,conftest.map"
cat > conftest.map <<EOF
VERS_1 {
  global: *;
};
EOF
AC_LINK_IFELSE(AC_LANG_PROGRAM([], []),
  [VERSION_SCRIPT_FLAG=-Wl,--version-script,;
   AC_MSG_RESULT([yes (GNU style)])],
  [])
if test "x$VERSION_SCRIPT_FLAG" = "x"; then
  LDFLAGS="$SAVED_LDFLAGS -Wl,-M,conftest.map"
  AC_LINK_IFELSE(AC_LANG_PROGRAM([], []),
    [VERSION_SCRIPT_FLAG=-Wl,-M,;
     AC_MSG_RESULT([yes (Sun style)])],
    [])
fi
if test "x$VERSION_SCRIPT_FLAG" = "x"; then
  VERSION_SCRIPT=no
  AC_MSG_RESULT(no)
fi
LDFLAGS="$SAVED_LDFLAGS"

AC_MSG_CHECKING([whether to use version script when building libjpeg-turbo])
AC_MSG_RESULT($VERSION_SCRIPT)

AM_CONDITIONAL(VERSION_SCRIPT, test "x$VERSION_SCRIPT" = "xyes")
AC_SUBST(VERSION_SCRIPT_FLAG)

# Check for non-broken inline under various spellings
AC_MSG_CHECKING(for inline)
ljt_cv_inline=""
AC_TRY_COMPILE(, [} __attribute__((always_inline)) int foo() { return 0; }
int bar() { return foo();], ljt_cv_inline="__attribute__((always_inline))",
AC_TRY_COMPILE(, [} __inline__ int foo() { return 0; }
int bar() { return foo();], ljt_cv_inline="__inline__",
AC_TRY_COMPILE(, [} __inline int foo() { return 0; }
int bar() { return foo();], ljt_cv_inline="__inline",
AC_TRY_COMPILE(, [} inline int foo() { return 0; }
int bar() { return foo();], ljt_cv_inline="inline"))))
AC_MSG_RESULT($ljt_cv_inline)
AC_DEFINE_UNQUOTED([INLINE],[$ljt_cv_inline],[How to obtain function inlining.])

# Arithmetic coding support
AC_MSG_CHECKING([whether to include arithmetic encoding support])
AC_ARG_WITH([arith-enc],
  AC_HELP_STRING([--without-arith-enc],
    [Do not include arithmetic encoding support]))
if test "x$with_arith_enc" = "xno"; then
  AC_MSG_RESULT(no)
  RPM_CONFIG_ARGS="$RPM_CONFIG_ARGS --without-arith-enc"
else
  AC_DEFINE([C_ARITH_CODING_SUPPORTED], [1], [Support arithmetic encoding])
  AC_MSG_RESULT(yes)
fi
AM_CONDITIONAL([WITH_ARITH_ENC], [test "x$with_arith_enc" != "xno"])

AC_MSG_CHECKING([whether to include arithmetic decoding support])
AC_ARG_WITH([arith-dec],
  AC_HELP_STRING([--without-arith-dec],
    [Do not include arithmetic decoding support]))
if test "x$with_arith_dec" = "xno"; then
  AC_MSG_RESULT(no)
  RPM_CONFIG_ARGS="$RPM_CONFIG_ARGS --without-arith-dec"
else
  AC_DEFINE([D_ARITH_CODING_SUPPORTED], [1], [Support arithmetic decoding])
  AC_MSG_RESULT(yes)
fi
AM_CONDITIONAL([WITH_ARITH_DEC], [test "x$with_arith_dec" != "xno"])

AM_CONDITIONAL([WITH_ARITH],
  [test "x$with_arith_dec" != "xno" -o "x$with_arith_enc" != "xno"])

# TurboJPEG support
AC_MSG_CHECKING([whether to build TurboJPEG C wrapper])
AC_ARG_WITH([turbojpeg],
  AC_HELP_STRING([--without-turbojpeg],
    [Do not include the TurboJPEG wrapper library and associated test programs]))
if test "x$with_turbojpeg" = "xno"; then
  AC_MSG_RESULT(no)
  RPM_CONFIG_ARGS="$RPM_CONFIG_ARGS --without-turbojpeg"
else
  AC_MSG_RESULT(yes)
fi

# Java support
AC_ARG_VAR(JAVAC, [Java compiler command (default: javac)])
if test "x$JAVAC" = "x"; then
  JAVAC=javac
fi
AC_SUBST(JAVAC)
AC_ARG_VAR(JAVACFLAGS, [Java compiler flags])
AC_SUBST(JAVACFLAGS)
AC_ARG_VAR(JAR, [Java archive command (default: jar)])
if test "x$JAR" = "x"; then
  JAR=jar
fi
AC_SUBST(JAR)
AC_ARG_VAR(JAVA, [Java runtime command (default: java)])
if test "x$JAVA" = "x"; then
  JAVA=java
fi
AC_SUBST(JAVA)
AC_ARG_VAR(JNI_CFLAGS,
  [C compiler flags needed to include jni.h (default: -I/System/Library/Frameworks/JavaVM.framework/Headers on OS X, '-I/usr/java/include -I/usr/java/include/solaris' on Solaris, and '-I/usr/java/default/include -I/usr/java/default/include/linux' on Linux)])

AC_MSG_CHECKING([whether to build TurboJPEG Java wrapper])
AC_ARG_WITH([java],
  AC_HELP_STRING([--with-java], [Build Java wrapper for the TurboJPEG library]))
if test "x$with_turbojpeg" = "xno"; then
  with_java=no
fi

WITH_JAVA=0
if test "x$with_java" = "xyes"; then
  AC_MSG_RESULT(yes)

  case $host_os in
    darwin*)
      DEFAULT_JNI_CFLAGS=-I/System/Library/Frameworks/JavaVM.framework/Headers
      ;;
    solaris*)
      DEFAULT_JNI_CFLAGS='-I/usr/java/include -I/usr/java/include/solaris'
      ;;
    linux*)
      DEFAULT_JNI_CFLAGS='-I/usr/java/default/include -I/usr/java/default/include/linux'
      ;;
  esac
  if test "x$JNI_CFLAGS" = "x"; then
    JNI_CFLAGS=$DEFAULT_JNI_CFLAGS
  fi

  SAVE_CPPFLAGS=${CPPFLAGS}
  CPPFLAGS="${CPPFLAGS} ${JNI_CFLAGS}"
  AC_CHECK_HEADERS([jni.h], [DUMMY=1],
    [AC_MSG_ERROR([Could not find JNI header file])])
  CPPFLAGS=${SAVE_CPPFLAGS}
  AC_SUBST(JNI_CFLAGS)

  RPM_CONFIG_ARGS="$RPM_CONFIG_ARGS --with-java"
  JAVA_RPM_CONTENTS_1='%dir %{_datadir}/classes'
  JAVA_RPM_CONTENTS_2=%{_datadir}/classes/turbojpeg.jar
  WITH_JAVA=1
else
  AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([WITH_JAVA], [test "x$with_java" = "xyes"])
AC_SUBST(WITH_JAVA)
AC_SUBST(JAVA_RPM_CONTENTS_1)
AC_SUBST(JAVA_RPM_CONTENTS_2)

# optionally force using gas-preprocessor.pl for compatibility testing
AC_ARG_WITH([gas-preprocessor],
  AC_HELP_STRING([--with-gas-preprocessor],
    [Force using gas-preprocessor.pl on ARM.]))
if test "x${with_gas_preprocessor}" = "xyes"; then
  case $host_os in
    darwin*)
      CCAS="gas-preprocessor.pl -fix-unreq $CC"
      ;;
    *)
      CCAS="gas-preprocessor.pl -no-fix-unreq $CC"
      ;;
  esac
  AC_SUBST([CCAS])
fi

# SIMD is optional
AC_ARG_WITH([simd],
  AC_HELP_STRING([--without-simd], [Do not include SIMD extensions]))
if test "x${with_simd}" != "xno"; then
  # Check if we're on a supported CPU
  AC_MSG_CHECKING([if we have SIMD optimisations for cpu type])
  case "$host_cpu" in
    x86_64 | amd64)
      AC_MSG_RESULT([yes (x86_64)])
      AC_PROG_NASM
      simd_arch=x86_64
      ;;
    i*86 | x86 | ia32)
      AC_MSG_RESULT([yes (i386)])
      AC_PROG_NASM
      simd_arch=i386
      ;;
    arm*)
      AC_MSG_RESULT([yes (arm)])
      AC_MSG_CHECKING([if the assembler is GNU-compatible and can be used])
      AC_CHECK_COMPATIBLE_ARM_ASSEMBLER_IFELSE(
        [AC_MSG_RESULT([yes])
         simd_arch=arm],
        [AC_MSG_RESULT([no])
         with_simd=no
         AC_MSG_WARN([SIMD support can't be enabled.  Performance will suffer.])])
      ;;
    *)
      AC_MSG_RESULT([no ("$host_cpu")])
      AC_MSG_WARN([SIMD support not available for this CPU.  Performance will suffer.])
      with_simd=no;
      ;;
  esac

  if test "x${with_simd}" != "xno"; then
    AC_DEFINE([WITH_SIMD], [1], [Use accelerated SIMD routines.])
  fi
else
  RPM_CONFIG_ARGS="$RPM_CONFIG_ARGS --without-simd"
fi

AM_CONDITIONAL([WITH_SIMD], [test "x$with_simd" != "xno"])
AM_CONDITIONAL([WITH_SSE_FLOAT_DCT], [test "x$simd_arch" = "xx86_64" -o "x$simd_arch" = "xi386"])
AM_CONDITIONAL([SIMD_I386], [test "x$simd_arch" = "xi386"])
AM_CONDITIONAL([SIMD_X86_64], [test "x$simd_arch" = "xx86_64"])
AM_CONDITIONAL([SIMD_ARM], [test "x$simd_arch" = "xarm"])
AM_CONDITIONAL([X86_64], [test "x$host_cpu" = "xx86_64" -o "x$host_cpu" = "xamd64"])
AM_CONDITIONAL([WITH_TURBOJPEG], [test "x$with_turbojpeg" != "xno"])

AC_ARG_VAR(PKGNAME, [distribution package name (default: libjpeg-turbo)])
if test "x$PKGNAME" = "x"; then
  PKGNAME=$PACKAGE_NAME
fi
AC_SUBST(PKGNAME)

case "$host_cpu" in
  x86_64)
    RPMARCH=x86_64
    DEBARCH=amd64
    ;;
  i*86 | x86 | ia32)
    RPMARCH=i386
    DEBARCH=i386
    ;;
esac

AC_SUBST(RPMARCH)
AC_SUBST(RPM_CONFIG_ARGS)
AC_SUBST(DEBARCH)
AC_SUBST(BUILD)
AC_DEFINE_UNQUOTED([BUILD], "$BUILD", [Build number])

# jconfig.h is the file we use, but we have another before that to
# fool autoheader. the reason is that we include this header in our
# API headers, which can screw things up for users of the lib.
# jconfig.h is a minimal version that allows this package to be built
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_HEADERS([jconfig.h])
AC_CONFIG_FILES([pkgscripts/libjpeg-turbo.spec.tmpl:release/libjpeg-turbo.spec.in])
AC_CONFIG_FILES([pkgscripts/makecygwinpkg.tmpl:release/makecygwinpkg.in])
AC_CONFIG_FILES([pkgscripts/makedpkg.tmpl:release/makedpkg.in])
AC_CONFIG_FILES([pkgscripts/makemacpkg.tmpl:release/makemacpkg.in])
AC_CONFIG_FILES([pkgscripts/Description.plist:release/Description.plist.in])
AC_CONFIG_FILES([pkgscripts/Info.plist:release/Info.plist.in])
AC_CONFIG_FILES([pkgscripts/uninstall.tmpl:release/uninstall.in])
if test "x$with_turbojpeg" != "xno"; then
  AC_CONFIG_FILES([tjbenchtest])
fi
if test "x$with_java" = "xyes"; then
  AC_CONFIG_FILES([tjbenchtest.java])
  AC_CONFIG_FILES([tjexampletest])
fi
AC_CONFIG_FILES([libjpeg.map])
AC_CONFIG_FILES([Makefile simd/Makefile])
AC_CONFIG_FILES([java/Makefile])
AC_CONFIG_FILES([md5/Makefile])
AC_OUTPUT
