#!/bin/sh
# Run this to set up the build system: configure, makefiles, etc.

package="jbig2dec"
AUTOMAKE_FLAGS="$AUTOMAKE_FLAGS"

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

cd "$srcdir"

echo "checking for autoconf... "
(autoconf --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have autoconf installed to compile $package."
        echo "Download the appropriate package for your distribution,"
        echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	exit 1
}

VERSIONGREP="sed -e s/.*[^0-9\.]\([0-9][0-9]*\.[0-9][0-9]*\).*/\1/"
VERSIONMKMAJ="sed -e s/\([0-9][0-9]*\)[^0-9].*/\\1/"
VERSIONMKMIN="sed -e s/.*[0-9][0-9]*\.//"

JBIG2VERSIONGREP="sed -e s/^.*(\([0-9]\+\)).*/\\1/"
JBIG2MAJOR=$(grep 'define JBIG2_VERSION_MAJOR' jbig2.h | $JBIG2VERSIONGREP)
JBIG2MINOR=$(grep 'define JBIG2_VERSION_MINOR' jbig2.h | $JBIG2VERSIONGREP)
sed -e "s/^\(AC_INIT[^,]*,\)[^,]*\(,.*\)$/\1 [$JBIG2MAJOR.$JBIG2MINOR]\2/" configure.ac.in > configure.ac

# do we need automake?
if test "x$USE_OLD" = "xyes" ; then
  if test -r Makefile.am; then
    AM_OPTIONS=`fgrep AUTOMAKE_OPTIONS Makefile.am`
    AM_NEEDED=`echo $AM_OPTIONS | $VERSIONGREP`
    if test "x$AM_NEEDED" = "x$AM_OPTIONS"; then
      AM_NEEDED=""
    fi
    if test -z "$AM_NEEDED"; then
      echo -n "checking for automake... "
      AUTOMAKE=automake
      ACLOCAL=aclocal
      if ($AUTOMAKE --version < /dev/null > /dev/null 2>&1); then
        echo "yes"
      else
        echo "no"
        AUTOMAKE=
      fi
    else
      echo -n "checking for automake $AM_NEEDED or later... "
      majneeded=`echo $AM_NEEDED | $VERSIONMKMAJ`
      minneeded=`echo $AM_NEEDED | $VERSIONMKMIN`
      for am in automake-$AM_NEEDED automake$AM_NEEDED automake \
          automake-1.7 automake-1.8 automake-1.9 automake-1.10; do
        ($am --version < /dev/null > /dev/null 2>&1) || continue
        ver=`$am --version < /dev/null | head -n 1 | $VERSIONGREP`
        maj=`echo $ver | $VERSIONMKMAJ`
        min=`echo $ver | $VERSIONMKMIN`
        if test $maj -eq $majneeded -a $min -ge $minneeded; then
          AUTOMAKE=$am
          echo $AUTOMAKE
          break
        fi
      done
      test -z $AUTOMAKE &&  echo "no"
      echo -n "checking for aclocal $AM_NEEDED or later... "
      for ac in aclocal-$AM_NEEDED aclocal$AM_NEEDED aclocal\
          aclocal-1.7 aclocal-1.8 aclocal-1.9 aclocal-1.10; do
        ($ac --version < /dev/null > /dev/null 2>&1) || continue
        ver=`$ac --version < /dev/null | head -n 1 | $VERSIONGREP`
        maj=`echo $ver | $VERSIONMKMAJ`
        min=`echo $ver | $VERSIONMKMIN`
        if test $maj -eq $majneeded -a $min -ge $minneeded; then
          ACLOCAL=$ac
          echo $ACLOCAL
          break
        fi
      done
      test -z $ACLOCAL && echo "no"
    fi
    test -z $AUTOMAKE || test -z $ACLOCAL && {
          echo
          echo "You must have automake installed to compile $package."
          echo "Download the appropriate package for your distribution,"
          echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
          exit 1
    }
  fi
else
  AUTOMAKE=automake
  ACLOCAL=aclocal
  AM_VER=`$AUTOMAKE --version | grep "automake (GNU automake)" | sed 's/[^0-9\.]*//g'`
  AM_MAJ=`echo $AM_VER |cut -d. -f1`
  AM_MIN=`echo $AM_VER |cut -d. -f2`
  AM_PAT=`echo $AM_VER |cut -d. -f3`

  AM_NEEDED=`fgrep AUTOMAKE_OPTIONS Makefile.am | $VERSIONGREP`
  AM_MAJOR_REQ=`echo $AM_NEEDED |cut -d. -f1`
  AM_MINOR_REQ=`echo $AM_NEEDED |cut -d. -f2`
  
  echo "checking for automake $AM_NEEDED or later..."

  if [ $AM_MAJ -lt $AM_MAJOR_REQ -o $AM_MIN -lt $AM_MINOR_REQ ] ; then
    echo
    echo "You must have automake $AM_NEEDED or better installed to compile $package."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
    exit 1
  fi
fi

# do we need libtool?
if ! test -z `grep -l -s -e PROG_LIBTOOL configure.ac configure.in`; then
  echo -n "Checking for libtoolize... "
  LIBTOOLIZE=
  for lt in glibtoolize libtoolize; do
    if ($lt --version < /dev/null > /dev/null 2>&1); then
      LIBTOOLIZE=$lt
      echo $lt
      break;
    fi
  done
  if test -z $LIBTOOLIZE; then
        echo
        echo "You must have GNU libtool installed to compile $package."
        echo "Download the appropriate package for your distribution,"
        echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	exit 1
  fi
fi

echo "Generating configuration files for $package, please wait...."

echo "  $ACLOCAL $ACLOCAL_FLAGS"
$ACLOCAL $ACLOCAL_FLAGS

echo "  $LIBTOOLIZE"
$LIBTOOLIZE --copy

echo "  autoheader"
autoheader

echo "  creating config_types.h.in"
cat >config_types.h.in <<EOF
/*
   generated header with missing types for the
   jbig2dec program and library. include this
   after config.h, within the HAVE_CONFIG_H
   ifdef
*/

#ifdef HAVE_STDINT_H
#  include <stdint.h>
#else
#  ifdef JBIG2_REPLACE_STDINT_H
#   include <@JBIG2_STDINT_H@>
#  else
    typedef unsigned @JBIG2_INT32_T@ uint32_t;
    typedef unsigned @JBIG2_INT16_T@ uint16_t;
    typedef unsigned @JBIG2_INT8_T@ uint8_t;
    typedef signed @JBIG2_INT32_T@ int32_t;
    typedef signed @JBIG2_INT16_T@ int16_t;
    typedef signed @JBIG2_INT8_T@ int8_t;
#  endif /* JBIG2_REPLACE_STDINT */
#endif /* HAVE_STDINT_H */
EOF

echo "  $AUTOMAKE --add-missing $AUTOMAKE_FLAGS"
$AUTOMAKE --add-missing --copy $AUTOMAKE_FLAGS

echo "  autoconf"
autoconf

if test -z "$*"; then
        echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
else
	echo "running ./configure $@"
fi

$srcdir/configure "$@" && echo
