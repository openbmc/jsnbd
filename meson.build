project('bmcweb', 'cpp', 'c',
        version : '1.0',
        meson_version: '>=0.63.0',
        default_options: [
            'b_ndebug=if-release',
            'cpp_rtti=false',
            'cpp_std=c++20',
            'warning_level=3',
            'werror=true',
           ])

bindir = get_option('prefix') + '/' + get_option('bindir')
sharedstatedir = get_option('prefix') + '/' + get_option('sharedstatedir')
sysconfdir = get_option('prefix') + '/' + get_option('sysconfdir')

cxx = meson.get_compiler('cpp')
#json_c = cxx.find_library('libjson-c')
# to allow #include <json.h>.  Remove when autotools is removed.
#json_inc = get_option('prefix') + '/' + get_option('includedir') + '/json-c'

json_c = dependency('json-c', include_type: 'system')

udev = cxx.find_library('udev')

conf_data = configuration_data()
conf_h_dep = declare_dependency(
    include_directories: include_directories('.'),
    sources: configure_file(
        input: 'meson-config.h.in',
        output: 'config.h',
        configuration: conf_data
    )
)
add_project_arguments([
  '-DRUNSTATEDIR="' + sharedstatedir + '/run"',
  '-DSYSCONFDIR="' + sysconfdir + '"',
  '-Wno-pedantic',
], language : 'c')

subdir('newnbd')

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
