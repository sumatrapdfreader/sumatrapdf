#
# Top-Level Makefile for building LittleCMS 2
#

# Don't require all the GNU mandated files
AUTOMAKE_OPTIONS = 1.7.2 dist-zip foreign

ACLOCAL_AMFLAGS = -I m4

PACKAGE_NAME = @PACKAGE_NAME@
PACKAGE_VERSION = @PACKAGE_VERSION@

if COND_FASTFLOAT
    USER_PLUGINS = plugins
endif

# Directories containing Makefiles to 'make'
SUBDIRS = src include utils/tificc utils/transicc utils/linkicc utils/jpgicc utils/psicc testbed $(USER_PLUGINS)

# Additional files to distribute
EXTRA_DIST = AUTHORS COPYING ChangeLog doc Projects include bin Lib INSTALL README.1ST autogen.sh lcms2.pc.in plugins

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lcms2mt.pc


# Make sure get rid of VC stuff...
clean-local:
	rm -rf autom4te.cache
	find Projects -name "*.user" | xargs rm -rf
	find Projects -name "Release" | xargs rm -rf
	find Projects -name "Debug" | xargs rm -rf
	find Projects -name "*.aps" | xargs rm -rf
	find Projects -name "*.suo" | xargs rm -rf
	find Projects -name "*.log" | xargs rm -rf
	find Projects -name "*.sdf" | xargs rm -rf
	find Projects -name "*.opensdf" | xargs rm -rf
	find Projects -name "ipch" | xargs rm -rf
