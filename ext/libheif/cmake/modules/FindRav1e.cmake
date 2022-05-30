include(LibFindMacros)
libfind_pkg_check_modules(RAV1E_PKGCONF rav1e)

find_path(RAV1E_INCLUDE_DIR
    NAMES rav1e.h
    HINTS ${RAV1E_PKGCONF_INCLUDE_DIRS} ${RAV1E_PKGCONF_INCLUDEDIR}
    PATH_SUFFIXES RAV1E
)

find_library(RAV1E_LIBRARY
    NAMES librav1e rav1e rav1e.dll
    HINTS ${RAV1E_PKGCONF_LIBRARY_DIRS} ${RAV1E_PKGCONF_LIBDIR}
)

set(RAV1E_PROCESS_LIBS RAV1E_LIBRARY)
set(RAV1E_PROCESS_INCLUDES RAV1E_INCLUDE_DIR)
libfind_process(RAV1E)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RAV1E
    REQUIRED_VARS
        RAV1E_INCLUDE_DIR
        RAV1E_LIBRARIES
)
