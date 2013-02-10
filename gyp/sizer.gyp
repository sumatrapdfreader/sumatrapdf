{
  'targets': [
    {
      'target_name': 'sizer',
      'type': 'executable',
      'msvs_configuration_attributes': {
        'CharacterSet': '1'
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarningLevel': '3',
          'WarnAsError': 'false',
          'DebugInformationFormat': '3',
          'ExceptionHandling': '1',
          'EnableFunctionLevelLinking': 'true',
          'OmitFramePointers': 'false',
          'RuntimeTypeInfo': 'false',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'DataExecutionPrevention': '2',
          'EnableCOMDATFolding': '2',
          'OptimizeReferences': '2',
          'SubSystem': '1' # console, 2 = windows
        },
      },
      'sources': [
        '../tools/sizer/debuginfo.cpp',
        '../tools/sizer/debuginfo.hpp',
        '../tools/sizer/main.cpp',
        '../tools/sizer/pdbfile.cpp',
        '../tools/sizer/pdbfile.hpp',
        '../tools/sizer/types.hpp',
      ],

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
  ],
}
