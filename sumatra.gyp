{
    'includes': ['gyp/common.gypi', 'gyp/zlib.gyp', 'gyp/openjpeg.gyp'],

    'variables': {
    },

    'targets': [
        {
            'target_name': 'test_util',
            'type': 'executable',

            'include_dirs': [
                'src/utils',
            ],
            'sources': [
              "src/AppUtil.h",
              "src/AppUtil.cpp",
              "src/utils/BaseUtil.h",
              "src/utils/BaseUtil.cpp",
              "src/utils/BitManip.h",
              "src/utils/ByteOrderDecoder.h",
              "src/utils/ByteOrderDecoder.cpp",
              "src/utils/CmdLineParser.h",
              "src/utils/CmdLineParser.cpp",
              "src/utils/CryptoUtil.h",
              "src/utils/CryptoUtil.cpp",
              "src/utils/CssParser.h",
              "src/utils/CssParser.cpp",
              "src/utils/Dict.h",
              "src/utils/Dict.cpp",
              "src/utils/DebugLog.h",
              "src/utils/DebugLog.cpp",
              "src/utils/FileUtil.h",
              "src/utils/FileUtil.cpp",
              "src/utils/GeomUtil.h",
              "src/utils/HtmlParserLookup.h",
              "src/utils/HtmlParserLookup.cpp",
              "src/utils/HtmlPrettyPrint.h",
              "src/utils/HtmlPrettyPrint.cpp",
              "src/utils/HtmlPullParser.h",
              "src/utils/HtmlPullParser.cpp",
              "src/utils/JsonParser.h",
              "src/utils/JsonParser.cpp",
              "src/utils/Scoped.h",
              "src/utils/SettingsUtil.h",
              "src/utils/SettingsUtil.cpp",
              "src/utils/SimpleLog.h",
              "src/utils/StrFormat.h",
              "src/utils/StrFormat.cpp",
              "src/utils/StrUtil.h",
              "src/utils/StrUtil.cpp",
              "src/utils/SquareTreeParser.h",
              "src/utils/SquareTreeParser.cpp",
              "src/utils/TrivialHtmlParser.h",
              "src/utils/TrivialHtmlParser.cpp",
              "src/utils/UtAssert.h",
              "src/utils/UtAssert.cpp",
              "src/utils/VarintGob.h",
              "src/utils/VarintGob.cpp",
              "src/utils/Vec.h",
              "src/utils/WinCursors.h",
              "src/utils/WinCursors.cpp",
              "src/utils/WinUtil.h",
              "src/utils/WinUtil.cpp",
              "src/utils/tests/BaseUtil_ut.cpp",
              "src/utils/tests/ByteOrderDecoder_ut.cpp",
              "src/utils/tests/CmdLineParser_ut.cpp",
              "src/utils/tests/CryptoUtil_ut.cpp",
              "src/utils/tests/CssParser_ut.cpp",
              "src/utils/tests/Dict_ut.cpp",
              "src/utils/tests/FileUtil_ut.cpp",
              "src/utils/tests/HtmlPrettyPrint_ut.cpp",
              "src/utils/tests/HtmlPullParser_ut.cpp",
              "src/utils/tests/JsonParser_ut.cpp",
              "src/utils/tests/SettingsUtil_ut.cpp",
              "src/utils/tests/SimpleLog_ut.cpp",
              "src/utils/tests/SquareTreeParser_ut.cpp",
              "src/utils/tests/StrFormat_ut.cpp",
              "src/utils/tests/StrUtil_ut.cpp",
              "src/utils/tests/TrivialHtmlParser_ut.cpp",
              "src/utils/tests/VarintGob_ut.cpp",
              "src/utils/tests/Vec_ut.cpp",
              "src/utils/tests/WinUtil_ut.cpp",
              "src/UnitTests.cpp",
              "src/mui/SvgPath.h",
              "src/mui/SvgPath.cpp",
              "src/mui/SvgPath_ut.cpp",
              "tools/tests/UnitMain.cpp",
            ],
            'defines': [
                'NO_LIBMUPDF'
            ],
            'link_settings': {
                'libraries': [
                    'gdiplus.lib',
                    'comctl32.lib',
                    'shlwapi.lib',
                    'Version.lib',
                    'user32.lib',
                    'kernel32.lib',
                    'gdi32.lib',
                    'ole32.lib',
                    'advapi32.lib',
                    'shell32.lib',
                    'oleaut32.lib',
                    'winspool.lib',
                ]
            },
            'msvs_settings':
            {
              'VCLinkerTool':
              {
                'SubSystem': '1',   # Console
              },
            },
        },

        {
            'target_name': 'dummy_test',
            'type': 'executable',

            'dependencies': [
                "zlib",
                "openjpeg",
            ],
            'include_dirs': [
                'src/zlib',
                'src/openjpeg',
            ],
            'sources': [
              "gyp/dummy_main.cpp",
            ],
            'defines': [
            ],
            'link_settings': {
                'libraries': [
                ]
            },
            'msvs_settings':
            {
              'VCLinkerTool':
              {
                'SubSystem': '1',   # Console
              },
            },
        },


    ],
}
