google_benchmark = subproject('google-benchmark')
google_benchmark_dep = google_benchmark.get_variable('google_benchmark_dep')

benchmark('benchmark-font', executable('benchmark-font', 'benchmark-font.cc',
  dependencies: [
    google_benchmark_dep, freetype_dep,
  ],
  cpp_args: [],
  include_directories: [incconfig, incsrc],
  link_with: [libharfbuzz],
  install: false,
), workdir: meson.current_source_dir() / '..', timeout: 100)

benchmark('benchmark-map', executable('benchmark-map', 'benchmark-map.cc',
  dependencies: [
    google_benchmark_dep,
  ],
  cpp_args: [],
  include_directories: [incconfig, incsrc],
  link_with: [libharfbuzz],
  install: false,
), workdir: meson.current_source_dir() / '..', timeout: 100)

benchmark('benchmark-ot', executable('benchmark-ot', 'benchmark-ot.cc',
  dependencies: [
    google_benchmark_dep,
  ],
  cpp_args: [],
  include_directories: [incconfig, incsrc],
  link_with: [libharfbuzz],
  install: false,
), workdir: meson.current_source_dir() / '..', timeout: 100)

benchmark('benchmark-set', executable('benchmark-set', 'benchmark-set.cc',
  dependencies: [
    google_benchmark_dep,
  ],
  cpp_args: [],
  include_directories: [incconfig, incsrc],
  link_with: [libharfbuzz],
  install: false,
), workdir: meson.current_source_dir() / '..', timeout: 100)

benchmark('benchmark-shape', executable('benchmark-shape', 'benchmark-shape.cc',
  dependencies: [
    google_benchmark_dep, freetype_dep,
  ],
  cpp_args: [],
  include_directories: [incconfig, incsrc],
  link_with: [libharfbuzz],
  install: false,
), workdir: meson.current_source_dir() / '..', timeout: 100)

benchmark('benchmark-subset', executable('benchmark-subset', 'benchmark-subset.cc',
  dependencies: [
    google_benchmark_dep,
  ],
  cpp_args: [],
  include_directories: [incconfig, incsrc],
  link_with: [libharfbuzz, libharfbuzz_subset],
  install: false,
), workdir: meson.current_source_dir() / '..', timeout: 100)
