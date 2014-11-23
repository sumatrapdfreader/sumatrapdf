{
    'targets': [
        {
            'target_name': 'zlib',
            'type': 'static_library',
            'include_dirs': [
                "../ext/zlib",
            ],
            'direct_dependent_settings': {
                'include_dirs': [
                    "../ext/zlib",
                ],
            },
            'msvs_disabled_warnings': [4996],
            'sources': [
                "../ext/zlib/adler32.c",
                "../ext/zlib/compress.c",
                "../ext/zlib/crc32.c",
                "../ext/zlib/crc32.h",
                "../ext/zlib/deflate.c",
                "../ext/zlib/deflate.h",
                "../ext/zlib/gzclose.c",
                "../ext/zlib/gzguts.h",
                "../ext/zlib/gzlib.c",
                "../ext/zlib/gzread.c",
                "../ext/zlib/gzwrite.c",
                "../ext/zlib/inffast.c",
                "../ext/zlib/inffast.h",
                "../ext/zlib/inffixed.h",
                "../ext/zlib/inflate.c",
                "../ext/zlib/inflate.h",
                "../ext/zlib/inftrees.c",
                "../ext/zlib/inftrees.h",
                "../ext/zlib/trees.c",
                "../ext/zlib/trees.h",
                "../ext/zlib/zconf.h",
                "../ext/zlib/zlib.h",
                "../ext/zlib/zutil.c",
                "../ext/zlib/zutil.h",
            ],
        }
    ],
}
