project(
  'jsnbd',
  'c',
  version: '1.0',
  meson_version: '>=0.63.0',
  default_options: [
    'b_ndebug=if-release',
    'cpp_rtti=false',
    'cpp_std=c++20',
    'warning_level=3',
    'werror=true',
  ],
)

if (get_option('nbd-proxy').enabled())
  subdir('nbd-proxy')
endif
