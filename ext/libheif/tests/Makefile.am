noinst_PROGRAMS =

if HAVE_TESTS
noinst_PROGRAMS += \
  heif-unit-tests

heif_unit_tests_DEPENDENCIES = ../libheif/libheif.la
heif_unit_tests_CXXFLAGS = -I$(top_srcdir) -I$(top_builddir)/.
heif_unit_tests_LDFLAGS =
heif_unit_tests_LDADD = ../libheif/libheif.la
heif_unit_tests_SOURCES = main.cc encode.cc catch.hpp conversion.cc

test-local: heif-unit-tests
	./heif-unit-tests

endif

if HAVE_GO
noinst_PROGRAMS += \
	test-race

test_race_SOURCES = test-race.go

gopath:
	mkdir -p ${CURDIR}/src/github.com/strukturag/libheif
	ln -sf ${CURDIR}/../go ${CURDIR}/src/github.com/strukturag/libheif/

test-race: gopath $(top_builddir)/libheif/libheif.la $(top_builddir)/libheif.pc test-race.go
	GOPATH=${CURDIR} PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:$(abs_top_builddir):$(abs_top_builddir)/libde265/dist/lib/pkgconfig/ CGO_CFLAGS="-I$(abs_top_builddir)" CGO_LDFLAGS="-L$(abs_top_builddir)/libheif/.libs" LD_LIBRARY_PATH=$(abs_top_builddir)/libheif/.libs $(GO) build -o test-race ${test_race_SOURCES}

format-go: ${test_race_SOURCES}
	$(GO) fmt ${test_race_SOURCES}
else
format-go:
	echo ""go" not present in "${PATH}", skipping formatting"
endif

format-local: format-go
