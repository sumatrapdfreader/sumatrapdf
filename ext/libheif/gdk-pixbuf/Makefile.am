AUTOMAKE_OPTIONS = subdir-objects

gdk_pixbuf_module_LTLIBRARIES =

if HAVE_GDKPIXBUF2
gdk_pixbuf_module_LTLIBRARIES += libpixbufloader-heif.la
libpixbufloader_heif_la_DEPENDENCIES = ../libheif/libheif.la
libpixbufloader_heif_la_CFLAGS = -I$(top_srcdir) -I$(top_builddir) $(gdkpixbuf_CFLAGS)
libpixbufloader_heif_la_LIBADD = ../libheif/libheif.la $(gdkpixbuf_LIBS)
libpixbufloader_heif_la_LDFLAGS = -avoid-version -module
libpixbufloader_heif_la_SOURCES = pixbufloader-heif.c
endif

EXTRA_DIST = CMakeLists.txt
