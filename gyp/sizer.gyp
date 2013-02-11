{
  'includes': [
    'common_configs.gypi',
  ],
  'targets': [
    {
      'target_name': 'sizer',
      'product_name': 'sizer',
      'type': 'executable',
      'sources': [
        '../tools/sizer/DebugInfo.cpp',
        '../tools/sizer/DebugInfo.h',
        '../tools/sizer/main.cpp',
        '../tools/sizer/PdbFile.cpp',
        '../tools/sizer/PdbFile.h',
        '../tools/sizer/Types.h',
      ],
    },
  ],
}
