#
# Makefile for building fast_float_testbed
#

# Don't require all the GNU mandated files
AUTOMAKE_OPTIONS = 1.7 foreign

AM_CPPFLAGS	=  -I$(builddir)/../include -I$(srcdir)/../include -I$(srcdir)/../src \
                   -I$(top_builddir)/include

check_PROGRAMS = fast_float_testbed

fast_float_testbed_LDADD =  $(srcdir)/../src/liblcms2mt_fast_float.la
fast_float_testbed_LDFLAGS = @LDFLAGS@
fast_float_testbed_SOURCES = fast_float_testbed.c

EXTRA_DIST = test0.icc test1.icc test2.icc test3.icc test5.icc

check:
	./fast_float_testbed
