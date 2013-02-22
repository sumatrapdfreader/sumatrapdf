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
        "../src/utils/BitManip.h",
        "../src/utils/Dict.cpp",
        "../src/utils/Dict.h",
        "../src/utils/GeomUtil.h",
        "../src/utils/Scoped.h",
        "../src/utils/StrUtil.cpp",
        "../src/utils/StrUtil.h",
        "../src/utils/Vec.h",
      ],
    },
  ],
}
