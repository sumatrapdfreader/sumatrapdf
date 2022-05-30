include(LibFindMacros)
libfind_pkg_check_modules(DAV1D_PKGCONF dav1d)

find_path(DAV1D_INCLUDE_DIR
    NAMES dav1d/dav1d.h
    HINTS ${DAV1D_PKGCONF_INCLUDE_DIRS} ${DAV1D_PKGCONF_INCLUDEDIR}
    PATH_SUFFIXES DAV1D
)

find_library(DAV1D_LIBRARY
    NAMES libdav1d dav1d
    HINTS ${DAV1D_PKGCONF_LIBRARY_DIRS} ${DAV1D_PKGCONF_LIBDIR}
)

set(DAV1D_PROCESS_LIBS DAV1D_LIBRARY)
set(DAV1D_PROCESS_INCLUDES DAV1D_INCLUDE_DIR)
libfind_process(DAV1D)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DAV1D
    REQUIRED_VARS
        DAV1D_INCLUDE_DIR
        DAV1D_LIBRARIES
)
