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
        '../tools/sizer/debuginfo.cpp',
        '../tools/sizer/debuginfo.hpp',
        '../tools/sizer/main.cpp',
        '../tools/sizer/pdbfile.cpp',
        '../tools/sizer/pdbfile.hpp',
        '../tools/sizer/types.hpp',
      ],
    },
  ],
}
