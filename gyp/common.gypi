{
  'target_defaults': {
    'configurations': {
      'Debug': {
        'defines': [ '_DEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',
            'RuntimeLibrary': '1'  # /MTd
          },
        },
      },
      'Release': {
        'defines': [ 'NDEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'InlineFunctionExpansion': 2, # AnySuitable
            'EnableIntrinsicFunctions': 'true',
            'EnableFunctionLevelLinking': 'true',
            'FavorSizeOrSpeed': 1,      # Speed
            'OmitFramePointers': 'true',
            'Optimization': 2,          # MaxSpeed
            'RuntimeTypeInfo': 'false',
            'StringPooling': 'true',
            'WholeProgramOptimization': 'true',
            'RuntimeLibrary': '0',  # /MT
          },
          'VCLinkerTool': {
            'EnableCOMDATFolding': 2,   # /OPT:ICF
            'OptimizeReferences': 2,    # /OPT:REF
            'LinkTimeCodeGeneration': 1,# UseLinkTimeCodeGeneration
            'LinkIncremental': 1, # disable incremental linking
          },
        },
      },
    },
    'defines': ['_WINDOWS', 'WINVER=0x0501', '_CRT_SECURE_NO_DEPRECATE'],
    'msvs_configuration_attributes': {
      'CharacterSet': '1',  # Unicode
    },
    'msbuild_toolset': 'v120_xp',
    'msvs_cygwin_shell': 0, # TODO: don't know what that means
    'msvs_settings': {
      'VCCLCompilerTool': {
        'WarningLevel': 3,
        'OmitFramePointers': 'false',
        'DebugInformationFormat': 3, # Generate a PDB
        'BufferSecurityCheck': 'true',
        #'EnableIntrinsicFunctions': 'false'
        #'BasicRuntimeChecks': 3, # /RTC1
        #'FavorSizeOrSpeed': 1, # /Ot, favour speed over size
        #'InlineFunctionExpansion': 2, # /Ob2, inline anything eligible
        # 'ExceptionHandling': '2',  # /EHsc
        # 'WarnAsError': 'true'
        'AdditionalOptions': [ '/wd4800' ],
      },
      'VCLinkerTool': {
        'SubSystem': '2',   # Windows
        'GenerateDebugInformation': 'true',
        #'LinkIncremental': 2, # enable incremental linking
      },
      'VCMIDLTool': {
        'TypeLibraryName': '$(TargetName).tlb',
      },
    },
  },
}
