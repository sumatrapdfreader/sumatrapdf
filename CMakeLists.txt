# project is .sln
# each add_executable is .vcproj within .sln
#
# Note: cmake really likes to operate in terms of folders but I want a single
# CMakeLists.txt file (as opposed to putting it in every directory), so this
# might be different than how most people use it.
#
# TODO: I would like to move CMakeLists.txt to cmake directory, but then
# I would have to add "../" to every file path
#
# inspirations: http://fossies.org/windows/misc/tulip-4.5.0_src.zip/tulip/TulipUseFile.cmake
#

cmake_minimum_required(VERSION 2.8)

project(all)

include(CmakeLists-files.txt)


# flags for C++ code that applies to all targets
# /MP  : use muliple processors to speed up compilation
# /wd4800 - warning C4800: 'int' : forcing value to bool 'true' or 'false' (performance warning)
# /wd4996 - The POSIX name for this item is deprecated
# /wd4018 - warning C4018: '<' : signed/unsigned mismatch
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /wd4800 /wd4996")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP /wd4996 /wd4018")
# TODO: disable warnings in narrower scopes

# this also removes the default -D _MBCS
# TODO: this messes up unrar build
#add_definitions(-D_UNICODE)

# this doesn't remove _MBCS
#remove_definitions(-D _MBCS)
# another possibility
string (REPLACE "/D_MBCS" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string (REPLACE "/D_MBCS" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

MESSAGE("cmake is starting")
IF(MSVC)
	MESSAGE("msvc build")
	IF(CMAKE_CL_64)
		MESSAGE("64-bit build")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
		SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MACHINE:X64")
		SET(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MACHINE:X64")
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MACHINE:X64")
		SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /MACHINE:X64")
	ELSE(CMAKE_CL_64)
		#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
		SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MACHINE:X86")
		SET(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MACHINE:X86")
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MACHINE:X86")
		SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /MACHINE:X86")
	ENDIF(CMAKE_CL_64)
ENDIF(MSVC)

## --------------------------------------------------------------------

IF(NOT_DEFINED)
include_directories("tools/efi"
	"src/utils")

add_executable(efi ${EFI_SRC})
set_target_properties(
	efi
	PROPERTIES
	COMPILE_FLAGS "/wd4267"
)
ENDIF(NOT_DEFINED)

## --------------------------------------------------------------------

IF(NOT_DEFINED)
include_directories ("src/utils" "src/utils/msvc")
add_executable(plugin_test WIN32 ${PLUGIN_TEST_SRC})
target_link_libraries(plugin_test "shlwapi")
set_target_properties(
	plugin_test
	PROPERTIES
	COMPILE_FLAGS "/wd4244"
)
ENDIF(NOT_DEFINED)

## --------------------------------------------------------------------

IF(NOT_DEFINED)
add_library(bzip2 "ext/bzip2/bzip_all.c")
set_target_properties(
	bzip2
	PROPERTIES
	COMPILE_FLAGS "/wd4996  /wd4244 /wd4267"
)
# 4267 - possibly only needed for 64-bit build
ENDIF(NOT_DEFINED)

## --------------------------------------------------------------------

IF(NOT_DEFINED)
# TODO: per target
include_directories("ext/freetype2/include" "ext/freetype2/config")

add_library(ft ${FREETYPE2_SRC})
set_target_properties(
	ft
	PROPERTIES
	COMPILE_FLAGS "/wd4244 /wd4703 /wd4996"
	COMPILE_DEFINITIONS "FT2_BUILD_LIBRARY;FT_OPTION_AUTOFIT2"
)
ENDIF(NOT_DEFINED)

## --------------------------------------------------------------------

IF(NOT_DEFINED)
# TODO: per target
include_directories("ext/zlib" "ext/zlib/infback9")
add_library(zlib ${ZLIB_SRC})
ENDIF(NOT_DEFINED)


## --------------------------------------------------------------------

IF(NOT_DEFINED)
add_executable(all_tests ${ALL_TESTS_SRC})
IF(WIN32)
	target_link_libraries(all_tests "gdiplus" "comctl32" "shlwapi" "Version")
ENDIF()
set_target_properties(
	all_tests
	PROPERTIES
	COMPILE_FLAGS "/wd4996 /wd4244"
	COMPILE_DEFINITIONS "NO_LIBMUPDF"
)
# d4244 - possibly only needed for 64-bit build

#set_target_properties(all_tests PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
ENDIF(NOT_DEFINED)

## --------------------------------------------------------------------

IF(NOT_DEFINED)
# TODO: nasm support, see https://github.com/aumuell/libjpeg-turbo/blob/master/simd/CMakeLists.txt
# and https://github.com/aumuell/libjpeg-turbo/blob/master/CMakeLists.txt
# TODO: per target
include_directories("ext/libjpeg-turbo" "ext/libjpeg-turbo/simd")
add_library(jpegturbo ${LIBJPEG_TURBO_SRC} ${LIBJPEG_TURBO_SIMD_SRC})
ENDIF(NOT_DEFINED)

# TODO: per target
include_directories("ext/jbig2dec")
add_library(jbig2 ${JBIG2_SRC})
set_target_properties(
	jbig2
	PROPERTIES
	COMPILE_DEFINITIONS "HAVE_STRING_H=1;JBIG_NO_MEMENTO"
)
