# project is .sln
# each add_executable is .vcproj within .sln
#
# TODO: I would like to move CMakeLists.txt to cmake directory, but then
# I would have to add "../" to every file path

cmake_minimum_required(VERSION 2.8)

project(all)

include(CmakeLists-files.txt)

# flags for C++ code that applies to all targets
# /wd4800 -  warning C4800: 'int' : forcing value to bool 'true' or 'false' (performance warning)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800")

## --------------------------------------------------------------------

include_directories("tools/efi"
	"src/utils")

add_executable(efi ${EFI_SRC})
set_property(
	TARGET efi
	PROPERTY CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800"
)

## --------------------------------------------------------------------

add_definitions(-DNO_LIBMUPDF)

add_executable(all_tests ${ALL_TESTS_SRC})
IF(WIN32)
	target_link_libraries(all_tests "gdiplus" "comctl32" "shlwapi" "Version")
ENDIF()
#set_target_properties(all_tests PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")

## --------------------------------------------------------------------

include_directories ("src/utils" "src/utils/msvc")
add_executable(plugin_test WIN32 ${PLUGIN_TEST_SRC})
target_link_libraries(plugin_test "shlwapi")

## --------------------------------------------------------------------

add_library(bzip2 "ext/bzip2/bzip_all.c")

## --------------------------------------------------------------------

# TODO: those should be per-target
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

# TODO: those should be per-target
add_definitions(-DUNRAR -DRARDLL -DSILENT)

add_library(unrar ${UNRAR_SRC})
set_property(
	TARGET efi
	PROPERTY CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996"
)

