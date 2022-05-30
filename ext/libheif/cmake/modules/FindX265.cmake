include(LibFindMacros)
libfind_pkg_check_modules(X265_PKGCONF x265)

find_path(X265_INCLUDE_DIR
    NAMES x265.h
    HINTS ${X265_PKGCONF_INCLUDE_DIRS} ${X265_PKGCONF_INCLUDEDIR}
    PATH_SUFFIXES X265
)

find_library(X265_LIBRARY
    NAMES libx265 x265
    HINTS ${X265_PKGCONF_LIBRARY_DIRS} ${X265_PKGCONF_LIBDIR}
)

set(X265_PROCESS_LIBS X265_LIBRARY)
set(X265_PROCESS_INCLUDES X265_INCLUDE_DIR)
libfind_process(X265)

if(X265_INCLUDE_DIR)
  set(x265_config_file "${X265_INCLUDE_DIR}/x265_config.h")
  if(EXISTS ${x265_config_file})
      file(STRINGS
           ${x265_config_file}
           TMP
           REGEX "#define X265_BUILD .*$")
      string(REGEX REPLACE "#define X265_BUILD" "" TMP ${TMP})
      string(REGEX MATCHALL "[0-9.]+" X265_BUILD ${TMP})
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(X265
    REQUIRED_VARS
        X265_INCLUDE_DIR
        X265_LIBRARIES
    VERSION_VAR
        X265_BUILD
)
