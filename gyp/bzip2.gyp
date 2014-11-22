{
    'targets': [
        {
            'target_name': 'bzip2',
            'type': 'static_library',
            'include_dirs': [
                "../ext/bzip2",
            ],
            'msvs_disabled_warnings': [4996],
            'sources': [
                "../ext/bzip2/bzip_all.c",
            ],
        }
    ],
}
