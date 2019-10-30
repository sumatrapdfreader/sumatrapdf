#
# Makefile for building testcms
#

# Don't require all the GNU mandated files
AUTOMAKE_OPTIONS = 1.7 foreign

AM_CPPFLAGS    =  -I$(top_builddir)/include -I$(top_srcdir)/include -I$(top_srcdir)/src

check_PROGRAMS = testcms

# CFLAGS = --pedantic -Wall -std=c99 -O2

# The testsuite binary is statically linked. This is necessary as it uses some
# of the internal functions that are not necessarily exported by the shared
# library.
testcms_LDADD = $(top_builddir)/src/liblcms2.la 
testcms_LDFLAGS = -static @LDFLAGS@
testcms_SOURCES = testcms2.c testplugin.c zoo_icc.c testcms2.h

EXTRA_DIST = test1.icc bad.icc toosmall.icc test2.icc \
             test3.icc test4.icc \
             test5.icc ibm-t61.icc 

check:
	if [ $(top_srcdir) != $(top_builddir) ]; then \
		cp $(top_srcdir)/testbed/*.ic? $(top_builddir)/testbed; \
	fi
	LD_LIBRARY_PATH=$(top_builddir)/src/.libs ./testcms
	if [ $(top_srcdir) != $(top_builddir) ]; then \
		rm -f $(top_builddir)/testbed/*.ic?; \
	fi


