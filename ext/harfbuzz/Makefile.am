# Process this file with automake to produce Makefile.in

NULL =

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src util test docs

EXTRA_DIST = \
	autogen.sh \
	harfbuzz.doap \
	README.python.md \
	README.wine.md \
	BUILD.md \
	RELEASING.md \
	CMakeLists.txt \
	replace-enum-strings.cmake \
	$(NULL)

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(srcdir)/INSTALL \
	$(srcdir)/ChangeLog \
	$(srcdir)/gtk-doc.make \
	$(srcdir)/m4/gtk-doc.m4 \
	$(NULL)


#
# ChangeLog generation
#
CHANGELOG_RANGE =
ChangeLog: $(srcdir)/ChangeLog
$(srcdir)/ChangeLog:
	$(AM_V_GEN) if test -d "$(top_srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git \
	   $(GIT) log $(CHANGELOG_RANGE) --stat) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp "$(srcdir)/ChangeLog" \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> "$(srcdir)/$@")); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> "$(srcdir)/$@"); \
	fi
.PHONY: ChangeLog $(srcdir)/ChangeLog


#
# Release engineering
#

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gtk-doc \
	--disable-doc-cross-references \
	--with-gobject \
	--enable-introspection \
	$(NULL)

# TODO: Copy infrastructure from cairo

# TAR_OPTIONS is not set as env var for 'make dist'.  How to fix that?
TAR_OPTIONS = --owner=0 --group=0

dist-hook: dist-clear-sticky-bits
# Clean up any sticky bits we may inherit from parent dir
dist-clear-sticky-bits:
	chmod -R a-s $(distdir)


tar_file = $(PACKAGE_TARNAME)-$(VERSION).tar.bz2
sha256_file = $(tar_file).sha256
gpg_file = $(sha256_file).asc
$(sha256_file): $(tar_file)
	sha256sum $^ > $@
$(gpg_file): $(sha256_file)
	@echo "Please enter your GPG password to sign the checksum."
	gpg --armor --sign $^

release-files: $(tar_file) $(sha256_file) $(gpg_file)


-include $(top_srcdir)/git.mk
