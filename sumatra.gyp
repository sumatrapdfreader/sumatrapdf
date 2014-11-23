{
    'variables': {
        'nasm_dir': 'bin',
    },

    'includes': [
        'gyp/test_util.gyp',
        'gyp/common.gypi',
        'gyp/zlib.gyp',
        'gyp/openjpeg.gyp',
        'gyp/lzma.gyp',
        'gyp/jbig2dec.gyp',
        'gyp/freetype.gyp',
        'gyp/libwebp.gyp',
        'gyp/djvu.gyp',
        'gyp/bzip2.gyp',
        'ext/libjpeg-turbo/libjpeg.gyp',
        'gyp/chmlib.gyp',
        'gyp/utils.gyp',
    ],

    'targets': [
        {
            'target_name': 'dummy_test',
            'type': 'executable',

            'dependencies': [
                "zlib",
                "openjpeg",
                "lzma",
                "jbig2dec",
                "freetype",
                "libwebp",
                "djvu",
                "bzip2",
                "libjpeg",
                "chmlib",
                "utils",
            ],
            'include_dirs': [
                "ext/freetype2/config",
                "ext/freetype2/include",
                'ext/openjpeg',
                "ext/jbig2dec",
                "ext/lzma/C",
                'ext/libwebp',
                "ext/libdjvu",
            ],
            'sources': [
              "gyp/dummy_main.cpp",
            ],
            'defines': [
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


    ],
}
