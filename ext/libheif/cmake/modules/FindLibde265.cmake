include(LibFindMacros)
libfind_pkg_check_modules(LIBDE265_PKGCONF libde265)

find_path(LIBDE265_INCLUDE_DIR
    NAMES libde265/de265.h
    HINTS ${LIBDE265_PKGCONF_INCLUDE_DIRS} ${LIBDE265_PKGCONF_INCLUDEDIR}
    PATH_SUFFIXES DE265
)

find_library(LIBDE265_LIBRARY
    NAMES libde265 de265
    HINTS ${LIBDE265_PKGCONF_LIBRARY_DIRS} ${LIBDE265_PKGCONF_LIBDIR}
)

set(LIBDE265_PROCESS_LIBS ${LIBDE265_LIBRARY})
set(LIBDE265_PROCESS_INCLUDES ${LIBDE265_INCLUDE_DIR})
libfind_process(LIBDE265)

if(LIBDE265_INCLUDE_DIR)
  set(libde265_config_file "${LIBDE265_INCLUDE_DIR}/libde265/de265-version.h")
  if(EXISTS ${libde265_config_file})
      file(STRINGS
           ${libde265_config_file}
           TMP
           REGEX "#define LIBDE265_VERSION .*$")
      string(REGEX REPLACE "#define LIBDE265_VERSION" "" TMP ${TMP})
      string(REGEX MATCHALL "[0-9.]+" LIBDE265_VERSION ${TMP})
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBDE265
    REQUIRED_VARS
        LIBDE265_INCLUDE_DIRS
        LIBDE265_LIBRARIES
    VERSION_VAR
        LIBDE265_VERSION
)
