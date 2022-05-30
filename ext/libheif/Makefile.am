ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
    libheif \
    examples \
    extra \
    fuzzing \
    gdk-pixbuf \
    gnome \
    go \
    scripts \
    tests

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libheif.pc

EXTRA_DIST = \
    .github/workflows/*.yml \
    appveyor.yml \
    autogen.sh \
    CMakeLists.txt \
    CPPLINT.cfg \
    README.md \
    pre.js \
    post.js \
    build-emscripten.sh \
    cmake/modules/*.cmake
