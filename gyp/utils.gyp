{
  'includes': [
    'common_configs.gypi',
  ],
  'targets': [
    {
      'target_name': 'utils',
      'product_name': 'utils',
      'type': 'static_library',
      'standalone_static_library': 1, # no idea what that means
      'sources': [
        "../src/utils/Allocator.h",
        "../src/utils/BaseUtil.cpp",
        "../src/utils/BaseUtil.h",
        "../src/utils/StrUtil.cpp",
        "../src/utils/StrUtil.h",
        "../src/utils/Vec.h",
      ],
    },
  ],
}
