# project is .sln
# each add_executable is .vcproj within .sln
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
# /wd4800 -  warning C4800: 'int' : forcing value to bool 'true' or 'false' (performance warning)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800")

MESSAGE("cmake is starting")
IF(MSVC)
	MESSAGE("msvc build")
	IF(CMAKE_CL_64)
		MESSAGE("64-bit build")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
		SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}  /STACK:10000000 /MACHINE:X64")
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  /STACK:10000000 /MACHINE:X64")
		SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS}  /STACK:10000000 /MACHINE:X64")
	ELSE(CMAKE_CL_64)
		#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
		SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MACHINE:X86")
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MACHINE:X86")
		SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /MACHINE:X86")
	ENDIF(CMAKE_CL_64)
ENDIF(MSVC)

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
# TODO: doesn't seem to take in 64-bit build
set_property(
	TARGET bzip2
	PROPERTY CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996 /wd4244"
)

# TODO: is this needed?
set_property(
	TARGET bzip2
	PROPERTY CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996 /wd4244"
)

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

