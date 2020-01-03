cmake_minimum_required(VERSION 2.8.0)
project(harfbuzz)

enable_testing()

## Limit framework build to Xcode generator
if (BUILD_FRAMEWORK)
  # for a framework build on macOS, use:
  # cmake -DBUILD_FRAMEWORK=ON -Bbuild -H. -GXcode && cmake --build build
  if (NOT "${CMAKE_GENERATOR}" STREQUAL "Xcode")
    message(FATAL_ERROR
      "You should use Xcode generator with BUILD_FRAMEWORK enabled")
  endif ()
  set (CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD_32_64_BIT)")
  set (CMAKE_MACOSX_RPATH ON)
  set (BUILD_SHARED_LIBS ON)
endif ()


## Disallow in-source builds, as CMake generated make files can collide with autotools ones
if (NOT MSVC AND "${PROJECT_BINARY_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
  message(FATAL_ERROR
    "
In-source builds are not permitted!  Make a separate folder for"
    " building, e.g.,"
    "
  mkdir build; cd build; cmake .."
    "
Before that, remove the files created by this failed run with"
    "
  rm -rf CMakeCache.txt CMakeFiles")
endif ()


## HarfBuzz build configurations
option(HB_HAVE_FREETYPE "Enable freetype interop helpers" OFF)
option(HB_HAVE_GRAPHITE2 "Enable Graphite2 complementary shaper" OFF)
option(HB_HAVE_GLIB "Enable glib unicode functions" OFF)
option(HB_HAVE_ICU "Enable icu unicode functions" OFF)
if (APPLE)
  option(HB_HAVE_CORETEXT "Enable CoreText shaper backend on macOS" ON)
  set (CMAKE_MACOSX_RPATH ON)
endif ()
if (WIN32)
  option(HB_HAVE_UNISCRIBE "Enable Uniscribe shaper backend on Windows" OFF)
  option(HB_HAVE_GDI "Enable GDI integration helpers on Windows" OFF)
  option(HB_HAVE_DIRECTWRITE "Enable DirectWrite shaper backend on Windows" OFF)
endif ()
option(HB_BUILD_UTILS "Build harfbuzz utils, needs cairo, freetype, and glib properly be installed" OFF)
if (HB_BUILD_UTILS)
  set (HB_HAVE_GLIB ON)
  set (HB_HAVE_FREETYPE ON)
endif ()

option(HB_BUILD_SUBSET "Build harfbuzz-subset" ON)
option(HB_BUILD_TESTS "Build harfbuzz tests" ON)

option(HB_HAVE_GOBJECT "Enable GObject Bindings" OFF)
if (HB_HAVE_GOBJECT)
  set (HB_HAVE_GLIB ON)
endif ()

option(HB_HAVE_INTROSPECTION "Enable building introspection (.gir/.typelib) files" OFF)
if (HB_HAVE_INTROSPECTION)
  set (HB_HAVE_GOBJECT ON)
  set (HB_HAVE_GLIB ON)
endif ()

option(HB_CHECK OFF "Do a configuration suitable for testing (shared library and enable all options)")
if (HB_CHECK)
  set (BUILD_SHARED_LIBS ON)
  set (HB_BUILD_UTILS ON)
  set (HB_HAVE_ICU)
  set (HB_HAVE_GLIB ON)
  #set (HB_HAVE_GOBJECT ON)
  #set (HB_HAVE_INTROSPECTION ON)
  set (HB_HAVE_FREETYPE ON)
  set (HB_HAVE_GRAPHITE2 ON)
  if (WIN32)
    set (HB_HAVE_UNISCRIBE ON)
    set (HB_HAVE_GDI ON)
    set (HB_HAVE_DIRECTWRITE ON)
  elseif (APPLE)
    set (HB_HAVE_CORETEXT ON)
  endif ()
endif ()

include_directories(AFTER
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_BINARY_DIR}/src
)

# We need PYTHON_EXECUTABLE to be set for running the tests...
include (FindPythonInterp)

## Functions and headers
include (CheckFunctionExists)
include (CheckIncludeFile)
macro (check_funcs) # Similar to AC_CHECK_FUNCS of autotools
  foreach (func_name ${ARGN})
    string(TOUPPER ${func_name} definition_to_add)
    check_function_exists(${func_name} HAVE_${definition_to_add})
    if (${HAVE_${definition_to_add}})
      add_definitions(-DHAVE_${definition_to_add})
    endif ()
  endforeach ()
endmacro ()
if (UNIX)
  list(APPEND CMAKE_REQUIRED_LIBRARIES m)
endif ()
check_funcs(atexit mprotect sysconf getpagesize mmap isatty newlocale strtod_l roundf)
check_include_file(unistd.h HAVE_UNISTD_H)
if (${HAVE_UNISTD_H})
  add_definitions(-DHAVE_UNISTD_H)
endif ()
check_include_file(sys/mman.h HAVE_SYS_MMAN_H)
if (${HAVE_SYS_MMAN_H})
  add_definitions(-DHAVE_SYS_MMAN_H)
endif ()
check_include_file(xlocale.h HAVE_XLOCALE_H)
if (${HAVE_XLOCALE_H})
  add_definitions(-DHAVE_XLOCALE_H)
endif ()
check_include_file(stdbool.h HAVE_STDBOOL_H)
if (${HAVE_STDBOOL_H})
  add_definitions(-DHAVE_STDBOOL_H)
endif ()


if (MSVC)
  add_definitions(-wd4244 -wd4267 -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS)
endif ()


## Detect if we are running inside a distribution or regular repository folder
# if (EXISTS "${PROJECT_SOURCE_DIR}/ChangeLog")
#   # perhaps we are on dist directory
#   set (IN_HB_DIST TRUE)
#   #set (HB_VERSION_H "${PROJECT_SOURCE_DIR}/src/hb-version.h")
# endif ()


## Extract variables from Makefile files
function (extract_make_variable variable makefile_source)
  string(REGEX MATCH "${variable} = ([^$]+)\\$" temp "${makefile_source}")
  string(REGEX MATCHALL "[^ \n\t\\]+" listVar "${CMAKE_MATCH_1}")
  set (${variable} ${listVar} PARENT_SCOPE)
endfunction ()

# http://stackoverflow.com/a/27630120
function (add_prefix_to_list var prefix)
  set (listVar "")
  foreach (f ${${var}})
    list(APPEND listVar "${prefix}${f}")
  endforeach ()
  set (${var} "${listVar}" PARENT_SCOPE)
endfunction ()

file(READ ${PROJECT_SOURCE_DIR}/src/Makefile.sources SRCSOURCES)
file(READ ${PROJECT_SOURCE_DIR}/util/Makefile.sources UTILSOURCES)

extract_make_variable(HB_BASE_headers ${SRCSOURCES})
add_prefix_to_list(HB_BASE_headers "${PROJECT_SOURCE_DIR}/src/")

extract_make_variable(HB_SUBSET_sources ${SRCSOURCES})
add_prefix_to_list(HB_SUBSET_sources "${PROJECT_SOURCE_DIR}/src/")

extract_make_variable(HB_SUBSET_headers ${SRCSOURCES})
add_prefix_to_list(HB_SUBSET_headers "${PROJECT_SOURCE_DIR}/src/")

extract_make_variable(HB_BASE_RAGEL_GENERATED_sources ${SRCSOURCES})
#if (IN_HB_DIST)
  add_prefix_to_list(HB_BASE_RAGEL_GENERATED_sources "${PROJECT_SOURCE_DIR}/src/")
#else ()
#  add_prefix_to_list(HB_BASE_RAGEL_GENERATED_sources "${PROJECT_BINARY_DIR}/src/")
#endif ()

extract_make_variable(HB_VIEW_sources ${UTILSOURCES})
add_prefix_to_list(HB_VIEW_sources "${PROJECT_SOURCE_DIR}/util/")
extract_make_variable(HB_SHAPE_sources ${UTILSOURCES})
add_prefix_to_list(HB_SHAPE_sources "${PROJECT_SOURCE_DIR}/util/")
extract_make_variable(HB_SUBSET_CLI_sources ${UTILSOURCES})
add_prefix_to_list(HB_SUBSET_CLI_sources "${PROJECT_SOURCE_DIR}/util/")
extract_make_variable(HB_OT_SHAPE_CLOSURE_sources ${UTILSOURCES})
add_prefix_to_list(HB_OT_SHAPE_CLOSURE_sources "${PROJECT_SOURCE_DIR}/util/")


file(READ configure.ac CONFIGUREAC)
string(REGEX MATCH "\\[(([0-9]+)\\.([0-9]+)\\.([0-9]+))\\]" HB_VERSION_MATCH ${CONFIGUREAC})
set (HB_VERSION ${CMAKE_MATCH_1})
set (HB_VERSION_MAJOR ${CMAKE_MATCH_2})
set (HB_VERSION_MINOR ${CMAKE_MATCH_3})
set (HB_VERSION_MICRO ${CMAKE_MATCH_4})

## Define sources and headers of the project
set (project_sources ${PROJECT_SOURCE_DIR}/src/harfbuzz.cc) # use amalgam source
set (subset_project_sources ${HB_SUBSET_sources})
set (project_extra_sources)
set (project_headers ${HB_BASE_headers})
set (subset_project_headers ${HB_SUBSET_headers})

## Find and include needed header folders and libraries
if (HB_HAVE_FREETYPE)
  include (FindFreetype)
  if (NOT FREETYPE_FOUND)
    message(FATAL_ERROR "HB_HAVE_FREETYPE was set, but we failed to find it. Maybe add a CMAKE_PREFIX_PATH= to your Freetype2 install prefix")
  endif ()

  list(APPEND THIRD_PARTY_LIBS ${FREETYPE_LIBRARIES})
  include_directories(AFTER ${FREETYPE_INCLUDE_DIRS})
  add_definitions(-DHAVE_FREETYPE=1)

  list(APPEND project_headers ${PROJECT_SOURCE_DIR}/src/hb-ft.h)

  # So check_funcs can find its headers
  set (CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${FREETYPE_INCLUDE_DIRS})
  set (CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${FREETYPE_LIBRARIES})

  check_funcs(FT_Get_Var_Blend_Coordinates FT_Set_Var_Blend_Coordinates FT_Done_MM_Var)
endif ()

if (HB_HAVE_GRAPHITE2)
  add_definitions(-DHAVE_GRAPHITE2)

  find_path(GRAPHITE2_INCLUDE_DIR graphite2/Font.h)
  find_library(GRAPHITE2_LIBRARY graphite2)

  include_directories(${GRAPHITE2_INCLUDE_DIR})

  list(APPEND project_headers ${PROJECT_SOURCE_DIR}/src/hb-graphite2.h)

  list(APPEND THIRD_PARTY_LIBS ${GRAPHITE2_LIBRARY})

  mark_as_advanced(GRAPHITE2_INCLUDE_DIR GRAPHITE2_LIBRARY)
endif ()

if (HB_HAVE_GLIB)
  add_definitions(-DHAVE_GLIB)

  # https://github.com/WebKit/webkit/blob/master/Source/cmake/FindGLIB.cmake
  find_package(PkgConfig)
  pkg_check_modules(PC_GLIB QUIET glib-2.0)

  find_library(GLIB_LIBRARIES NAMES glib-2.0 HINTS ${PC_GLIB_LIBDIR} ${PC_GLIB_LIBRARY_DIRS})
  find_path(GLIBCONFIG_INCLUDE_DIR NAMES glibconfig.h HINTS ${PC_LIBDIR} ${PC_LIBRARY_DIRS} ${PC_GLIB_INCLUDEDIR} ${PC_GLIB_INCLUDE_DIRS} PATH_SUFFIXES glib-2.0/include)
  find_path(GLIB_INCLUDE_DIR NAMES glib.h HINTS ${PC_GLIB_INCLUDEDIR} ${PC_GLIB_INCLUDE_DIRS} PATH_SUFFIXES glib-2.0)

  include_directories(${GLIBCONFIG_INCLUDE_DIR} ${GLIB_INCLUDE_DIR})

  list(APPEND project_headers ${PROJECT_SOURCE_DIR}/src/hb-glib.h)

  list(APPEND THIRD_PARTY_LIBS ${GLIB_LIBRARIES})

  mark_as_advanced(GLIB_LIBRARIES GLIBCONFIG_INCLUDE_DIR GLIB_INCLUDE_DIR)
endif ()

if (HB_HAVE_ICU)
  add_definitions(-DHAVE_ICU)

  # https://github.com/WebKit/webkit/blob/master/Source/cmake/FindICU.cmake
  find_package(PkgConfig)
  pkg_check_modules(PC_ICU QUIET icu-uc)

  find_path(ICU_INCLUDE_DIR NAMES unicode/utypes.h HINTS ${PC_ICU_INCLUDE_DIRS} ${PC_ICU_INCLUDEDIR})
  find_library(ICU_LIBRARY NAMES libicuuc cygicuuc cygicuuc32 icuuc HINTS ${PC_ICU_LIBRARY_DIRS} ${PC_ICU_LIBDIR})

  include_directories(${ICU_INCLUDE_DIR})

  list(APPEND project_headers ${PROJECT_SOURCE_DIR}/src/hb-icu.h)

  list(APPEND THIRD_PARTY_LIBS ${ICU_LIBRARY})

  mark_as_advanced(ICU_INCLUDE_DIR ICU_LIBRARY)
endif ()

if (APPLE AND HB_HAVE_CORETEXT)
  # Apple Advanced Typography
  add_definitions(-DHAVE_CORETEXT)

  list(APPEND project_headers ${PROJECT_SOURCE_DIR}/src/hb-coretext.h)

  if (HB_IOS)
    find_library(COREFOUNDATION CoreFoundation)
    if (COREFOUNDATION)
      list(APPEND THIRD_PARTY_LIBS ${COREFOUNDATION})
    endif ()
    mark_as_advanced(COREFOUNDATION)

    find_library(CORETEXT CoreText)
    if (CORETEXT)
      list(APPEND THIRD_PARTY_LIBS ${CORETEXT})
    endif ()
    mark_as_advanced(CORETEXT)

    find_library(COREGRAPHICS CoreGraphics)
    if (COREGRAPHICS)
      list(APPEND THIRD_PARTY_LIBS ${COREGRAPHICS})
    endif ()
    mark_as_advanced(COREGRAPHICS)
  else ()
    find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices)
    if (APPLICATION_SERVICES_FRAMEWORK)
      list(APPEND THIRD_PARTY_LIBS ${APPLICATION_SERVICES_FRAMEWORK})
    endif ()

    mark_as_advanced(APPLICATION_SERVICES_FRAMEWORK)
  endif ()
endif ()

if (WIN32 AND HB_HAVE_GDI)
  add_definitions(-DHAVE_GDI)
  list(APPEND project_headers ${PROJECT_SOURCE_DIR}/src/hb-gdi.h)
  list(APPEND THIRD_PARTY_LIBS gdi32)
endif ()

if (WIN32 AND HB_HAVE_UNISCRIBE)
  add_definitions(-DHAVE_UNISCRIBE)
  list(APPEND project_headers ${PROJECT_SOURCE_DIR}/src/hb-uniscribe.h)
  list(APPEND THIRD_PARTY_LIBS usp10 gdi32 rpcrt4)
endif ()

if (WIN32 AND HB_HAVE_DIRECTWRITE)
  add_definitions(-DHAVE_DIRECTWRITE)
  list(APPEND project_headers ${PROJECT_SOURCE_DIR}/src/hb-directwrite.h)
  list(APPEND THIRD_PARTY_LIBS dwrite rpcrt4)
endif ()

if (HB_HAVE_GOBJECT)
  include (FindPerl)

  # Use the hints from glib-2.0.pc to find glib-mkenums
  find_package(PkgConfig)
  pkg_check_modules(PC_GLIB QUIET glib-2.0)
  find_program(GLIB_MKENUMS glib-mkenums
    HINTS ${PC_glib_mkenums}
  )
  set (GLIB_MKENUMS_CMD)

  if (WIN32 AND NOT MINGW)
    # In Visual Studio builds, shebang lines are not supported
    # in the standard cmd.exe shell that we use, so we need to
    # first determine whether glib-mkenums is a Python or PERL
    # script
    execute_process(COMMAND "${PYTHON_EXECUTABLE}" "${GLIB_MKENUMS}" --version
      RESULT_VARIABLE GLIB_MKENUMS_PYTHON
      OUTPUT_QUIET ERROR_QUIET
    )
    if (GLIB_MKENUMS_PYTHON EQUAL 0)
      message("${GLIB_MKENUMS} is a Python script.")
      set (GLIB_MKENUMS_CMD "${PYTHON_EXECUTABLE}" "${GLIB_MKENUMS}")
    else ()
      execute_process(COMMAND "${PERL_EXECUTABLE}" "${GLIB_MKENUMS}" --version
        RESULT_VARIABLE GLIB_MKENUMS_PERL
        OUTPUT_QUIET ERROR_QUIET
      )
      if (GLIB_MKENUMS_PERL EQUAL 0)
        message("${GLIB_MKENUMS} is a PERL script.")
        set (GLIB_MKENUMS_CMD "${PERL_EXECUTABLE}" "${GLIB_MKENUMS}")
      endif ()
      if (NOT GLIB_MKENUMS_PERL EQUAL 0 AND NOT GLIB_MKENUMS_PYTHON EQUAL 0)
        message(FATAL_ERROR "Unable to determine type of glib-mkenums script")
      endif ()
    endif ()
  else ()
    set (GLIB_MKENUMS_CMD "${GLIB_MKENUMS}")
  endif ()
  if (NOT GLIB_MKENUMS_CMD)
    message(FATAL_ERROR "HB_HAVE_GOBJECT was set, but we failed to find glib-mkenums, which is required")
  endif ()

  pkg_check_modules(PC_GOBJECT QUIET gobject-2.0)

  find_library(GOBJECT_LIBRARIES NAMES gobject-2.0 HINTS ${PC_GLIB_LIBDIR} ${PC_GLIB_LIBRARY_DIRS})
  find_path(GOBJECT_INCLUDE_DIR NAMES glib-object.h HINTS ${PC_GLIB_INCLUDEDIR} ${PC_GLIB_INCLUDE_DIRS} PATH_SUFFIXES glib-2.0)

  include_directories(${GOBJECTCONFIG_INCLUDE_DIR} ${GOBJECT_INCLUDE_DIR})
  mark_as_advanced(GOBJECT_LIBRARIES GOBJECT_INCLUDE_DIR)

  list(APPEND hb_gobject_sources ${PROJECT_SOURCE_DIR}/src/hb-gobject-structs.cc)
  list(APPEND hb_gobject_gen_sources
    ${CMAKE_CURRENT_BINARY_DIR}/src/hb-gobject-enums.cc
  )
  list(APPEND hb_gobject_structs_headers
    ${PROJECT_SOURCE_DIR}/src/hb-gobject-structs.h
  )
  list(APPEND hb_gobject_headers
    ${PROJECT_SOURCE_DIR}/src/hb-gobject.h
    ${hb_gobject_structs_headers}
  )
  list(APPEND hb_gobject_gen_headers
    ${CMAKE_CURRENT_BINARY_DIR}/src/hb-gobject-enums.h
  )

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/src/hb-gobject-enums.h
    COMMAND ${GLIB_MKENUMS_CMD}
      --template=${PROJECT_SOURCE_DIR}/src/hb-gobject-enums.h.tmpl
      --identifier-prefix hb_
      --symbol-prefix hb_gobject
      ${hb_gobject_structs_headers}
      ${project_headers}
      > ${CMAKE_CURRENT_BINARY_DIR}/src/hb-gobject-enums.h.tmp
    COMMAND "${CMAKE_COMMAND}"
      "-DENUM_INPUT_SRC=${CMAKE_CURRENT_BINARY_DIR}/src/hb-gobject-enums.h.tmp"
      "-DENUM_OUTPUT_SRC=${CMAKE_CURRENT_BINARY_DIR}/src/hb-gobject-enums.h"
      -P ${PROJECT_SOURCE_DIR}/replace-enum-strings.cmake
    DEPENDS ${PROJECT_SOURCE_DIR}/src/hb-gobject-enums.h.tmpl
      ${hb_gobject_header}
      ${project_headers}
  )

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/src/hb-gobject-enums.cc
    COMMAND ${GLIB_MKENUMS_CMD}
      --template=${PROJECT_SOURCE_DIR}/src/hb-gobject-enums.cc.tmpl
      --identifier-prefix hb_
      --symbol-prefix hb_gobject
      ${hb_gobject_header}
      ${project_headers}
      > ${CMAKE_CURRENT_BINARY_DIR}/src/hb-gobject-enums.cc.tmp
    COMMAND "${CMAKE_COMMAND}"
      "-DENUM_INPUT_SRC=${CMAKE_CURRENT_BINARY_DIR}/src/hb-gobject-enums.cc.tmp"
      "-DENUM_OUTPUT_SRC=${CMAKE_CURRENT_BINARY_DIR}/src/hb-gobject-enums.cc"
      -P ${PROJECT_SOURCE_DIR}/replace-enum-strings.cmake
    DEPENDS ${PROJECT_SOURCE_DIR}/src/hb-gobject-enums.cc.tmpl
      ${CMAKE_CURRENT_BINARY_DIR}/src/hb-gobject-enums.h
      ${hb_gobject_header}
      ${project_headers}
  )
endif ()

## Atomic ops availability detection
file(WRITE "${PROJECT_BINARY_DIR}/try_compile_intel_atomic_primitives.c"
"		void memory_barrier (void) { __sync_synchronize (); }
		int atomic_add (int *i) { return __sync_fetch_and_add (i, 1); }
		int mutex_trylock (int *m) { return __sync_lock_test_and_set (m, 1); }
		void mutex_unlock (int *m) { __sync_lock_release (m); }
		int main () { return 0; }
")
try_compile(HB_HAVE_INTEL_ATOMIC_PRIMITIVES
  ${PROJECT_BINARY_DIR}/try_compile_intel_atomic_primitives
  ${PROJECT_BINARY_DIR}/try_compile_intel_atomic_primitives.c)
if (HB_HAVE_INTEL_ATOMIC_PRIMITIVES)
  add_definitions(-DHAVE_INTEL_ATOMIC_PRIMITIVES)
endif ()

file(WRITE "${PROJECT_BINARY_DIR}/try_compile_solaris_atomic_ops.c"
"		#include <atomic.h>
		/* This requires Solaris Studio 12.2 or newer: */
		#include <mbarrier.h>
		void memory_barrier (void) { __machine_rw_barrier (); }
		int atomic_add (volatile unsigned *i) { return atomic_add_int_nv (i, 1); }
		void *atomic_ptr_cmpxchg (volatile void **target, void *cmp, void *newval) { return atomic_cas_ptr (target, cmp, newval); }
		int main () { return 0; }
")
try_compile(HB_HAVE_SOLARIS_ATOMIC_OPS
  ${PROJECT_BINARY_DIR}/try_compile_solaris_atomic_ops
  ${PROJECT_BINARY_DIR}/try_compile_solaris_atomic_ops.c)
if (HB_HAVE_SOLARIS_ATOMIC_OPS)
  add_definitions(-DHAVE_SOLARIS_ATOMIC_OPS)
endif ()


## Define harfbuzz library
add_library(harfbuzz ${project_sources} ${project_extra_sources} ${project_headers})
target_link_libraries(harfbuzz ${THIRD_PARTY_LIBS})

## Define harfbuzz-subset library
if (HB_BUILD_SUBSET)
  add_library(harfbuzz-subset ${subset_project_sources} ${subset_project_headers})
  add_dependencies(harfbuzz-subset harfbuzz)
  target_link_libraries(harfbuzz-subset harfbuzz ${THIRD_PARTY_LIBS})

  if (BUILD_SHARED_LIBS)
    set_target_properties(harfbuzz harfbuzz-subset PROPERTIES VISIBILITY_INLINES_HIDDEN TRUE)
  endif ()
endif ()

if (UNIX OR MINGW)
  # Make symbols link locally
  include (CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG(-Bsymbolic-functions CXX_SUPPORTS_FLAG_BSYMB_FUNCS)
  if (CXX_SUPPORTS_FLAG_BSYMB_FUNCS)
    link_libraries(-Bsymbolic-functions)
  endif ()

  if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Make sure we don't link to libstdc++
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
    set (CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "m") # libm
    set (CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")
    set_target_properties(harfbuzz PROPERTIES LINKER_LANGUAGE C)
    if (HB_BUILD_SUBSET)
      set_target_properties(harfbuzz-subset PROPERTIES LINKER_LANGUAGE C)
    endif ()

    # No threadsafe statics as we do it ourselves
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics")
  endif ()

  CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
  if (COMPILER_SUPPORTS_CXX11)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
  endif()
endif ()


## Define harfbuzz-gobject library
if (HB_HAVE_GOBJECT)
  add_library(harfbuzz-gobject
    ${hb_gobject_sources}
    ${hb_gobject_gen_sources}
    ${hb_gobject_headers}
    ${hb_gobject_gen_headers}
  )
  include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/src)
  add_dependencies(harfbuzz-gobject harfbuzz)
  target_link_libraries(harfbuzz-gobject harfbuzz ${GOBJECT_LIBRARIES} ${THIRD_PARTY_LIBS})

  if (BUILD_SHARED_LIBS)
    set_target_properties(harfbuzz-gobject PROPERTIES VISIBILITY_INLINES_HIDDEN TRUE)
  endif ()
endif ()

if (BUILD_SHARED_LIBS AND WIN32 AND NOT MINGW)
  add_definitions("-DHB_DLL_EXPORT")
endif ()

# On Windows, g-ir-scanner requires a DLL build in order for it to work
if (WIN32)
  if (NOT BUILD_SHARED_LIBS)
    message("Building introspection files on Windows requires BUILD_SHARED_LIBS to be enabled.")
    set (HB_HAVE_INTROSPECTION OFF)
  endif ()
endif ()

if (HB_HAVE_INTROSPECTION)
  find_package(PkgConfig)
  pkg_check_modules(PC_GI QUIET gobject-introspection-1.0)

  find_program(G_IR_SCANNER g-ir-scanner
    HINTS ${PC_g_ir_scanner}
  )

  find_program(G_IR_COMPILER g-ir-compiler
    HINTS ${PC_g_ir_compiler}
  )

  if (WIN32 AND NOT MINGW)
    # Note that since we already enable HB_HAVE_GOBJECT
    # we would already have PYTHON_EXECUTABLE handy
    set (G_IR_SCANNER_CMD "${PYTHON_EXECUTABLE}" "${G_IR_SCANNER}")
  else ()
    set (G_IR_SCANNER_CMD "${G_IR_SCANNER}")
  endif ()

  # We need to account for the varying output directories
  # when we build using Visual Studio projects
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio*")
    set (hb_libpath "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>")
  else ()
    set (hb_libpath "$<TARGET_FILE_DIR:harfbuzz-gobject>")
  endif ()

  # Get the CFlags that we used to build HarfBuzz/HarfBuzz-GObject
  set (hb_defines_cflags "")
  foreach (hb_cflag ${hb_cflags})
    list(APPEND hb_defines_cflags "-D${hb_cflag}")
  endforeach (hb_cflag)

  # Get the other dependent libraries we used to build HarfBuzz/HarfBuzz-GObject
  set (extra_libs "")
  foreach (extra_lib ${THIRD_PARTY_LIBS})
    # We don't want the .lib extension here...
    string(REPLACE ".lib" "" extra_lib_stripped "${extra_lib}")
    list(APPEND extra_libs "--extra-library=${extra_lib_stripped}")
  endforeach ()

  set (introspected_sources)
  foreach (f
    ${project_headers}
    ${project_sources}
    ${hb_gobject_gen_sources}
    ${hb_gobject_gen_headers}
    ${hb_gobject_sources}
    ${hb_gobject_headers}
  )
    if (WIN32)
      # Nasty issue: We need to make drive letters lower case,
      # otherwise g-ir-scanner won't like it and give us a bunch
      # of invalid items and unresolved types...
      STRING(SUBSTRING "${f}" 0 1 drive)
      STRING(SUBSTRING "${f}" 1 -1 path)
      if (drive MATCHES "[A-Z]")
        STRING(TOLOWER ${drive} drive_lower)
        list(APPEND introspected_sources "${drive_lower}${path}")
      else ()
        list(APPEND introspected_sources "${f}")
      endif ()
    else ()
      list(APPEND introspected_sources "${f}")
    endif ()
  endforeach ()

  file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/src/hb_gir_list)
  foreach (s ${introspected_sources})
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/src/hb_gir_list "${s}\n")
  endforeach ()

  # Finally, build the introspection files...
  add_custom_command(
    TARGET harfbuzz-gobject
    POST_BUILD
    COMMAND ${G_IR_SCANNER_CMD}
      --warn-all --no-libtool --verbose
      -n hb
      --namespace=HarfBuzz
      --nsversion=0.0
      --identifier-prefix=hb_
      --include GObject-2.0
      --pkg-export=harfbuzz
      --cflags-begin
      -I${PROJECT_SOURCE_DIR}/src
      -I${PROJECT_BINARY_DIR}/src
      ${hb_includedir_cflags}
      ${hb_defines_cflags}
      -DHB_H
      -DHB_H_IN
      -DHB_OT_H
      -DHB_OT_H_IN
      -DHB_AAT_H
      -DHB_AAT_H_IN
      -DHB_GOBJECT_H
      -DHB_GOBJECT_H_IN
      -DHB_EXTERN=
      --cflags-end
      --library=harfbuzz-gobject
      --library=harfbuzz
      -L${hb_libpath}
      ${extra_libs}
      --filelist ${CMAKE_CURRENT_BINARY_DIR}/src/hb_gir_list
      -o ${hb_libpath}/HarfBuzz-0.0.gir
    DEPENDS harfbuzz-gobject harfbuzz ${CMAKE_CURRENT_BINARY_DIR}/src/hb_gir_list
  )

  add_custom_command(
    TARGET harfbuzz-gobject
    POST_BUILD
    COMMAND "${G_IR_COMPILER}"
      --verbose --debug
      --includedir ${CMAKE_CURRENT_BINARY_DIR}
      ${hb_libpath}/HarfBuzz-0.0.gir
      -o ${hb_libpath}/HarfBuzz-0.0.typelib
    DEPENDS ${hb_libpath}/HarfBuzz-0.0.gir harfbuzz-gobject
  )
endif ()


## Additional framework build configs
if (BUILD_FRAMEWORK)
  set (CMAKE_MACOSX_RPATH ON)
  set_target_properties(harfbuzz PROPERTIES
    FRAMEWORK TRUE
    PUBLIC_HEADER "${project_headers}"
    XCODE_ATTRIBUTE_INSTALL_PATH "@rpath"
  )
  set (MACOSX_FRAMEWORK_IDENTIFIER "harfbuzz")
  set (MACOSX_FRAMEWORK_SHORT_VERSION_STRING "${HB_VERSION}")
  set (MACOSX_FRAMEWORK_BUNDLE_VERSION "${HB_VERSION}")
endif ()


## Additional harfbuzz build artifacts
if (HB_BUILD_UTILS)
  # https://github.com/WebKit/webkit/blob/master/Source/cmake/FindCairo.cmake
  find_package(PkgConfig)
  pkg_check_modules(PC_CAIRO QUIET cairo)

  find_path(CAIRO_INCLUDE_DIRS NAMES cairo.h HINTS ${PC_CAIRO_INCLUDEDIR} ${PC_CAIRO_INCLUDE_DIRS} PATH_SUFFIXES cairo)
  find_library(CAIRO_LIBRARIESNAMES cairo HINTS ${PC_CAIRO_LIBDIR} ${PC_CAIRO_LIBRARY_DIRS})

  add_definitions("-DPACKAGE_NAME=\"HarfBuzz\"")
  add_definitions("-DPACKAGE_VERSION=\"${HB_VERSION}\"")
  include_directories(${CAIRO_INCLUDE_DIRS})

  add_executable(hb-view ${HB_VIEW_sources})
  target_link_libraries(hb-view harfbuzz ${CAIRO_LIBRARIESNAMES})

  add_executable(hb-shape ${HB_SHAPE_sources})
  target_link_libraries(hb-shape harfbuzz)

  add_executable(hb-subset ${HB_SUBSET_CLI_sources})
  target_link_libraries(hb-subset harfbuzz harfbuzz-subset)

  add_executable(hb-ot-shape-closure ${HB_OT_SHAPE_CLOSURE_sources})
  target_link_libraries(hb-ot-shape-closure harfbuzz)

  mark_as_advanced(CAIRO_INCLUDE_DIRS CAIRO_LIBRARIESNAMES)
endif ()


## Install
include (GNUInstallDirs)

if (NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL)
  install(FILES ${project_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/harfbuzz)
  if (HB_HAVE_GOBJECT)
    install(FILES ${hb_gobject_headers} ${hb_gobject_gen_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/harfbuzz)
  endif ()
endif ()

if (NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
  install(TARGETS harfbuzz
    EXPORT harfbuzzConfig
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FRAMEWORK DESTINATION Library/Frameworks
  )
  install(EXPORT harfbuzzConfig
      NAMESPACE harfbuzz::
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/harfbuzz
  )
  if (HB_BUILD_UTILS)
    if (WIN32 AND BUILD_SHARED_LIBS)
      install(TARGETS harfbuzz-subset
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      )
    endif ()
    install(TARGETS hb-view
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(TARGETS hb-subset
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(TARGETS hb-shape
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(TARGETS hb-ot-shape-closure
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
  endif ()
  if (HB_HAVE_GOBJECT)
    install(TARGETS harfbuzz-gobject
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    if (HB_HAVE_INTROSPECTION)
      if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio*")
        set (hb_libpath "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>")
      else ()
        set (hb_libpath "$<TARGET_FILE_DIR:harfbuzz-gobject>")
      endif ()

      install(FILES "${hb_libpath}/HarfBuzz-0.0.gir"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/gir-1.0
      )

      install(FILES "${hb_libpath}/HarfBuzz-0.0.typelib"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/girepository-1.0
      )
    endif ()
  endif ()
endif ()

if (HB_BUILD_TESTS)
  ## src/ executables
  foreach (prog main test test-gsub-would-substitute test-gpos-size-params test-buffer-serialize test-unicode-ranges) # hb-ot-tag
    set (prog_name ${prog})
    if (${prog_name} STREQUAL "test")
      # test can not be used as a valid executable name on cmake, lets special case it
      set (prog_name test-test)
    endif ()
    add_executable(${prog_name} ${PROJECT_SOURCE_DIR}/src/${prog}.cc)
    target_link_libraries(${prog_name} harfbuzz ${THIRD_PARTY_LIBS})
  endforeach ()
  # set_target_properties(hb-ot-tag PROPERTIES COMPILE_FLAGS "-DMAIN")

  ## Tests
  if (UNIX OR MINGW)
    if (BUILD_SHARED_LIBS)
      # generate harfbuzz.def after build completion
      add_custom_command(TARGET harfbuzz POST_BUILD
        COMMAND "${PYTHON_EXECUTABLE}" ${PROJECT_SOURCE_DIR}/src/gen-def.py ${PROJECT_BINARY_DIR}/harfbuzz.def ${project_headers}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/src)

      add_test(NAME check-static-inits.sh
        COMMAND ${PROJECT_SOURCE_DIR}/src/check-static-inits.sh
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/harfbuzz.dir/src # ugly hack
      )
      add_test(NAME check-libstdc++.sh COMMAND ${PROJECT_SOURCE_DIR}/src/check-libstdc++.sh)
      add_test(NAME check-symbols.sh COMMAND ${PROJECT_SOURCE_DIR}/src/check-symbols.sh)

      set_tests_properties(
        check-static-inits.sh check-libstdc++.sh check-symbols.sh
        PROPERTIES
          ENVIRONMENT "libs=.;srcdir=${PROJECT_SOURCE_DIR}/src"
          SKIP_RETURN_CODE 77)
    endif ()

    add_test(NAME check-c-linkage-decls.sh COMMAND ./check-c-linkage-decls.sh)
    add_test(NAME check-header-guards.sh COMMAND ./check-header-guards.sh)
    add_test(NAME check-externs.sh COMMAND ./check-externs.sh)
    add_test(NAME check-includes.sh COMMAND ./check-includes.sh)
    set_tests_properties(
      check-c-linkage-decls.sh check-header-guards.sh check-externs.sh check-includes.sh
      PROPERTIES
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/src
        SKIP_RETURN_CODE 77)
  endif ()

  # Needs to come last so that variables defined above are passed to
  # subdirectories.
  add_subdirectory(test)
endif ()
