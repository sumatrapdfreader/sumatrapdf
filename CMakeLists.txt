# project is .sln
# each add_executable is .vcproj within .sln
#
# TODO: I would like to move CMakeLists.txt to cmake directory, but then
# I would have to add "../" to every file path
#
# TODO: maybe instead of file-per-set-of-sources, use a single file for all
# _SRC definitions

cmake_minimum_required(VERSION 2.8)

project(all)

# flags for C++ code that applies to all targets
# /wd4800 -  warning C4800: 'int' : forcing value to bool 'true' or 'false' (performance warning)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800")

## --------------------------------------------------------------------

include(CMakeLists-efi.txt)

include_directories("tools/efi"
	"src/utils")

add_executable(efi ${SRC})
set_property(
	TARGET efi
	PROPERTY CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800"
)

## --------------------------------------------------------------------

include(CMakeLists-all_tests.txt)

add_definitions(-DNO_LIBMUPDF)

add_executable(all_tests ${SRC})
IF(WIN32)
	target_link_libraries(all_tests "gdiplus" "comctl32" "shlwapi" "Version")
ENDIF()
#set_target_properties(all_tests PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")

## --------------------------------------------------------------------

include(CMakeLists-plugin_test.txt)
include_directories ("src/utils" "src/utils/msvc")
add_executable(plugin_test WIN32 ${SRC})
target_link_libraries(plugin_test "shlwapi")

## --------------------------------------------------------------------

add_library(bzip2 "ext/bzip2/bzip_all.c")

## --------------------------------------------------------------------

include(CMakeLists-unrar.txt)

# TODO: those should be per-target
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996")

# TODO: those should be per-target
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

# TODO: those should be per-target
add_definitions(-DUNRAR -DRARDLL -DSILENT)

add_library(unrar ${SRC})
