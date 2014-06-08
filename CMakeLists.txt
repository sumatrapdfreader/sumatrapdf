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
