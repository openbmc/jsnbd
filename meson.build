project(
  'jsnbd',
  'c',
  version : '1.0',
  meson_version: '>=0.63.0',
  default_options: [
    'b_ndebug=if-release',
    'cpp_rtti=false',
    'cpp_std=c++20',
    'warning_level=3',
    'werror=true',
  ]
)

bindir = get_option('prefix') + '/' + get_option('bindir')
localstatedir = get_option('localstatedir')
sysconfdir = get_option('sysconfdir')

c = meson.get_compiler('c')

json_c = dependency('json-c', include_type: 'system')
udev = c.find_library('udev')

conf_data = configuration_data()

if c.has_header_symbol('fcntl.h', 'splice', args: '-D_GNU_SOURCE')
  conf_data.set('HAVE_SPLICE', 1)
endif

conf_data.set('RUNSTATEDIR', '"' + localstatedir + '/run"')
conf_data.set('SYSCONFDIR', '"' + sysconfdir + '"')

conf_h_dep = declare_dependency(
    sources: configure_file(
        output: 'config.h',
        configuration: conf_data
    )
)

add_project_arguments([
  '-Wno-pedantic',
], language : 'c')

executable(
  'nbd-proxy',
  'nbd-proxy.c',
  dependencies: [
    json_c,
    conf_h_dep,
    udev,
  ],
  install: true,
  install_dir: bindir
)
