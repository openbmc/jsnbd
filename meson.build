project(
  'jsnbd',
  ['c', 'cpp'],
  version: '1.0',
  meson_version: '>=1.1.1',
  default_options: [
    'b_ndebug=if-release',
    'cpp_rtti=false',
    'cpp_std=c++23',
    'warning_level=3',
    'werror=true',
    'b_lto=true',
  ],
  license: 'Apache-2.0',
)

if (get_option('nbd-proxy')).enabled()
  add_project_arguments(
    [
      '-Wno-pedantic',
    ],
    language: 'c',
  )
endif

if (get_option('virtual-media').enabled())
  if get_option('cpp_std') != 'c++23'
    error('This project requires c++23 support')
  endif

  cxx = meson.get_compiler('cpp')

  add_project_arguments(
    cxx.get_supported_arguments(
      [
        '-Wold-style-cast',
        '-Wcast-align',
        '-Wunused',
        '-Wconversion',
        '-Wsign-conversion',
        '-Wno-attributes',
      ],
    ),
    language: 'cpp',
  )

  if (cxx.get_id() == 'clang' and cxx.version().version_compare('>9.0'))
    add_project_arguments(
      cxx.get_supported_arguments(
        [
          '-Weverything',
          '-Wno-c++98-compat',
          '-Wno-c++98-compat-pedantic',
          '-Wno-global-constructors',
          '-Wno-exit-time-destructors',
          '-Wno-shadow',
          '-Wno-shadow-field',
          '-Wno-used-but-marked-unused',
          '-Wno-documentation-unknown-command',
          '-Wno-weak-vtables',
          '-Wno-documentation',
          '-Wno-padded',
          '-Wno-undef',
          '-Wno-overloaded-virtual',
          '-Wno-unused-macros',
          '-Wno-ctad-maybe-unsupported',
          '-Wno-unused-private-field',
          '-Wunused-parameter',
          '-Wcovered-switch-default',
          '-Wcomma',
          '-Wextra-semi',
          '-Wzero-as-null-pointer-constant',
          '-Wswitch-enum',
          '-Wnull-dereference',
          '-Wdouble-promotion',
          '-Wformat=2',
        ],
      ),
      language: 'cpp',
    )
  endif

  if (cxx.get_id() == 'gcc' and cxx.version().version_compare('>8.0'))
    add_project_arguments(
      cxx.get_supported_arguments(
        [
          '-Wduplicated-cond',
          '-Wduplicated-branches',
          '-Wlogical-op',
          '-Wunused-parameter',
          '-Wnull-dereference',
          '-Wdouble-promotion',
          '-Wformat=2',
        ],
      ),
      language: 'cpp',
    )
  endif

  add_project_arguments(
    cxx.get_supported_arguments(
      [
        '-DBOOST_ASIO_DISABLE_THREADS',
        '-DBOOST_ASIO_NO_DEPRECATED',
        '-DBOOST_NO_RTTI',
        '-DBOOST_NO_TYPEID',
        '-DBOOST_ALLOW_DEPRECATED_HEADERS',
        '-DBOOST_SYSTEM_NO_DEPRECATED',
      ],
    ),
    language: 'cpp',
  )
endif

if (get_option('nbd-proxy').enabled())
  subdir('nbd-proxy')
endif

if (get_option('virtual-media').enabled())
  subdir('virtual-media')
endif
