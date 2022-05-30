EXTRA_DIST = \
	heif/heif.go \
	heif/heif_test.go \
	heif/keepalive_16.go \
	heif/keepalive_17.go

gopath:
	ln -sf ${CURDIR} ${CURDIR}/src

if HAVE_GO
test-go: gopath $(top_builddir)/libheif/libheif.la $(top_builddir)/libheif.pc
	GOPATH=${CURDIR} PKG_CONFIG_PATH=$(abs_top_builddir):$(abs_top_builddir)/libde265/dist/lib/pkgconfig/ CGO_CFLAGS="-I$(abs_top_builddir)" CGO_LDFLAGS="-L$(abs_top_builddir)/libheif/.libs" LD_LIBRARY_PATH=$(abs_top_builddir)/libheif/.libs $(GO) test -v heif

format-go: gopath
	GOPATH=${CURDIR} $(GO) fmt heif
else
test-go:
	echo ""go" not present in "${PATH}", skipping tests"

format-go:
	echo ""go" not present in "${PATH}", skipping formatting"
endif

test-local: test-go

format-local: format-go
