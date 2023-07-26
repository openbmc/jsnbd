systemd = dependency('systemd', required: true)
boost = dependency('boost', version: '>=1.82')

add_project_arguments(
  [
    '-DBOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT',
    '-DBOOST_ASIO_DISABLE_THREADS',
    '-DBOOST_NO_RTTI',
    '-DBOOST_NO_TYPEID',
    '-DBOOST_ALLOW_DEPRECATED_HEADERS'
  ],
  language : 'cpp'
)

sdbusplus = dependency('sdbusplus', required: false)

systemd_system_unit_dir = systemd.get_variable(pkgconfig:
    'systemdsystemunitdir'
)

configure_file(
  input: 'xyz.openbmc_project.VirtualMedia.service',
  output: 'xyz.openbmc_project.VirtualMedia.service',
  install_dir: systemd_system_unit_dir,
  copy: true,
  install: true,
)

configure_file(
  input : 'config/config.json',
  output : 'config.json',
  copy : true,
  install_dir: get_option('sysconfdir') + '/virtual-media',
  install : true,
)

bindir = get_option('prefix') + '/' +get_option('libexecdir')

executable(
  'virtual-media',
  'src/main.cpp',
  dependencies: [
    boost,
    sdbusplus,
    systemd,
    udev,
  ],
  include_directories: 'src',
  install: true,
  install_dir: bindir
)

#Tests are placed in the tests folder, with it's own meson.build
if (get_option('tests').enabled())
    subdir('tests')
endif
