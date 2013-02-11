{
  'target_defaults': {
    'configurations': {
      'Debug': {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'BufferSecurityCheck': 'true',
            'Optimization': '0'
          },
        },
        'defines': [
          'DEBUG',
          '_CRT_SECURE_NO_WARNINGS',
        ],
      },
      'Release': {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'BufferSecurityCheck': 'true',
            'Optimization': '1'
          },
        },
        'defines': [
          'NDEBUG',
          '_CRT_SECURE_NO_WARNINGS',
          ],
      },
    },
  },
}
