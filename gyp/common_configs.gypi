{
  'target_defaults': {
    'msvs_configuration_attributes': {
      'CharacterSet': '1'
    },
    'msvs_settings': {
      'VCCLCompilerTool': {
        'WarningLevel': '3',              # /W3
        'WarnAsError': 'false',
        'DebugInformationFormat': '3',    # /Zi
        'ExceptionHandling': '0',
        #'EnableFunctionLevelLinking': 'true',
        'OmitFramePointers': 'false',
        'RuntimeTypeInfo': 'false',     # # /GR-, RTTI disabled
      },
      'VCLinkerTool': {
        'GenerateDebugInformation': 'true', # /DEBUG
        #  'DataExecutionPrevention': '2',
        #  'EnableCOMDATFolding': '2',
        #  'OptimizeReferences': '2',
        #  'SubSystem': '1' # console, 2 = windows
        },
    },
    'configurations': {
      'Debug': {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'BufferSecurityCheck': 'true',
            'Optimization': '0', # /O0
            'PreprocessorDefinitions': ['DEBUG', '_CRT_SECURE_NO_WARNINGS'],
            'RuntimeLibrary': '1',          # /MTd, static debug
          },
        },
      },
      'Release': {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'BufferSecurityCheck': 'true',
            'Optimization': '3',            # /Ox
            'PreprocessorDefinitions': ['NDEBUG', '_CRT_SECURE_NO_WARNINGS'],
            'RuntimeLibrary': '0',          # /MT, static
          },
        },
      },
    },
  },
}
