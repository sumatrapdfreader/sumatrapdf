#
# Makefile for building threaded_testbed
#

# Don't require all the GNU mandated files
AUTOMAKE_OPTIONS = 1.7 foreign

AM_CPPFLAGS	=  -I$(builddir)/../include -I$(srcdir)/../include -I$(srcdir)/../src \
                   -I$(top_builddir)/include

check_PROGRAMS = threaded_testbed

threaded_testbed_LDADD =  $(builddir)/../src/liblcms2_threaded.la $(LCMS_LIB_DEPLIBS)
threaded_testbed_LDFLAGS = -static @LDFLAGS@
threaded_testbed_SOURCES = threaded_testbed.c

EXTRA_DIST = test0.icc test1.icc test2.icc test3.icc test5.icc

check:
	if [ "x$(srcdir)" != "x$(builddir)" ]; then \
		cp $(srcdir)/test?.icc . ; \
	fi
	./threaded_testbed
