# project is .sln
# each add_executable is .vcproj within .sln
#
# TODO: is there a way to avoid "../" at the beginning of each file?

cmake_minimum_required(VERSION 2.8)

project(all)

file (GLOB EFI_SRC
	"tools/efi/*.cpp"
	"tools/efi/*.h"
	"src/utils/BaseUtil*"
	"src/utils/BitManip.h"
	"src/utils/Dict*"
	"src/utils/StrUtil*"
	)

include_directories("tools/efi"
	"src/utils")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800")

add_executable(efi ${EFI_SRC})

add_definitions(-DNO_LIBMUPDF)

file (GLOB TEST_UTIL_SRC
	"src/utils/BaseUtil*"
	"src/utils/BencUtil*"
	"src/utils/BitManip*"
	"src/utils/ByteOrderDecoder*"
	"src/utils/CmdLineParser*"
	"src/utils/CryptoUtil*"
	"src/utils/CssParser*"
	"src/utils/Dict*"
	"src/utils/DebugLog*"
	"src/utils/FileUtil*"
	"src/utils/HtmlParserLookup*"
	"src/utils/HtmlPrettyPrint*"
	"src/utils/HtmlPullParser*"
	"src/utils/JsonParser*"
	"src/utils/SettingsUtil*"
	"src/utils/SimpleLog*"
	"src/utils/StrFormat*"
	"src/utils/StrUtil*"
	"src/utils/SquareTreeParser*"
	"src/utils/TrivialHtmlParser*"
	"src/utils/UtAssert*"
	"src/utils/VarintGob*"
	"src/utils/WinUtil*"
	"src/utils/tests/*"
	"src/AppUtil*"
	"src/UnitTests.cpp"
	"src/mui/SvgPath*"
	"tools/tests/UnitMain.cpp"
	)

add_executable(all_tests ${TEST_UTIL_SRC})
target_link_libraries(all_tests "gdiplus" "comctl32" "shlwapi" "Version")

file (GLOB PLUGIN_TEST_SRC
	"src/utils/BaseUtil.*"
	"src/utils/CmdLineParser.*"
	"src/utils/FileUtil.*"
	"src/utils/GeomUtil.*"
	"src/utils/Scoped.*"
	"src/utils/StrUtil.*"
	"src/utils/Vec.*"
	"tools/plugin-test/plugin-test.cpp"
)

include_directories ("src/utils" "src/utils/msvc")
add_executable(plugin_test WIN32 ${PLUGIN_TEST_SRC})
target_link_libraries(plugin_test "shlwapi")

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996")

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

add_library(bzip2 "ext/bzip2/bzip_all.c")

add_definitions(-DUNRAR -DRARDLL -DSILENT)

file (GLOB UNRAR_SRC
	"ext/unrar/archive.*"
	"ext/unrar/arcread.*"
	"ext/unrar/blake2s.*"
	"ext/unrar/cmddata.*"
	"ext/unrar/consio.*"
	"ext/unrar/crc.*"
	"ext/unrar/crypt.*"
	"ext/unrar/encname.*"
	"ext/unrar/errhnd.*"
	"ext/unrar/extinfo.*"
	"ext/unrar/extract.*"
	"ext/unrar/filcreat.*"
	"ext/unrar/file.*"
	"ext/unrar/filefn.*"
	"ext/unrar/filestr.*"
	"ext/unrar/find.*"
	"ext/unrar/getbits.*"
	"ext/unrar/global.*"
	"ext/unrar/hash.*"
	"ext/unrar/headers.*"
	"ext/unrar/isnt.*"
	"ext/unrar/list.*"
	"ext/unrar/match.*"
	"ext/unrar/options.*"
	"ext/unrar/pathfn.*"
	"ext/unrar/qopen.*"
	"ext/unrar/rarvm.*"
	"ext/unrar/rawread.*"
	"ext/unrar/rdwrfn.*"
	"ext/unrar/recvol.*"
	"ext/unrar/rijndael.*"
	"ext/unrar/rs.*"
	"ext/unrar/rs16.*"
	"ext/unrar/secpassword.*"
	"ext/unrar/scantree.*"
	"ext/unrar/sha1.*"
	"ext/unrar/sha256.*"
	"ext/unrar/smallfn.*"
	"ext/unrar/strfn.*"
	"ext/unrar/strlist.*"
	"ext/unrar/system.*"
	"ext/unrar/threadpool.*"
	"ext/unrar/timefn.*"
	"ext/unrar/unicode.*"
	"ext/unrar/unpack.*"
	"ext/unrar/volume.*"
	"ext/unrar/dll.*"
)



add_library(unrar ${UNRAR_SRC})
