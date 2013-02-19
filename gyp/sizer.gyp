{
  'includes': [
    'common_configs.gypi',
  ],
  'targets': [
    {
      'target_name': 'sizer',
      'product_name': 'sizer',
      'type': 'executable',
      'include_dirs' : [
        '../src/utils',
      ],
      'dependencies': [
        'utils.gyp:utils',
      ],
      'sources': [
        '../tools/sizer/Dia2Subset.h',
        '../tools/sizer/main.cpp',
        '../tools/sizer/PdbFile.cpp',
        '../tools/sizer/PdbFile.h',
        '../tools/sizer/Util.cpp',
        '../tools/sizer/Util.h',
      ],
    },
  ],
}
