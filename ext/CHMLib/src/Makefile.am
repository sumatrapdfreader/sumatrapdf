lib_LTLIBRARIES=libchm.la
libchm_la_SOURCES=chm_lib.c lzx.c
libchm_la_LDFLAGS=-version-info 1
include_HEADERS=chm_lib.h lzx.h

if EXAMPLES
bin_PROGRAMS=chm_http enum_chmLib enumdir_chmLib extract_chmLib test_chmLib
enum_chmLib_SOURCES=enum_chmLib.c
enum_chmLib_LDADD=libchm.la
chm_http_SOURCES=chm_http.c
chm_http_LDADD=libchm.la
enumdir_chmLib_SOURCES=enumdir_chmLib.c
enumdir_chmLib_LDADD=libchm.la
extract_chmLib_SOURCES=extract_chmLib.c
extract_chmLib_LDADD=libchm.la
test_chmLib_SOURCES=test_chmLib.c
test_chmLib_LDADD=libchm.la
endif ##EXAMPLES
