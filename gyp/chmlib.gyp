{
    'targets': [
        {
            'target_name': 'chmlib',
            'type': 'static_library',
            'defines': [
                'PPC_BSTR',
                'UNICODE',
                '_UNICODE',
            ],
            'include_dirs': [
                "../ext/CHMLib",
            ],
            'direct_dependent_settings': {
                'include_dirs': [
                    "../ext/CHMLib/src",
                ],
            },
            'msvs_disabled_warnings': [4244, 4996, 4018],
            'sources': [
                "../ext/CHMLib/src/chm_lib.c",
                "../ext/CHMLib/src/chm_lib.h",
                "../ext/CHMLib/src/lzx.c",
                "../ext/CHMLib/src/lzx.h",
            ],
        }
    ],
}
