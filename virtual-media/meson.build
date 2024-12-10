build = get_option('buildtype')
optimization = get_option('optimization')
summary('Build Type', build, section: 'Build Info')
summary('Optimization', optimization, section: 'Build Info')

incdir = include_directories('src')

virtual_media_dependencies = []

systemd = dependency('systemd', required: true)
virtual_media_dependencies += systemd

# pkg-config for udev does not contain Libs variable
udev = dependency('udev', required: true)
virtual_media_dependencies += udev
# this will add appopriate udev library linkage to executable.
udev_lib_dep = declare_dependency(link_args: ['-ludev'])
virtual_media_dependencies += udev_lib_dep

boost = dependency(
    'boost',
    modules: ['coroutine'],
    required: false,
    include_type: 'system',
)
if boost.found()
    virtual_media_dependencies += boost
else
    cmake = import('cmake')
    opt = cmake.subproject_options()
    opt.add_cmake_defines(
        {
            'BOOST_INCLUDE_LIBRARIES': 'asio;beast;callable_traits;process;system',
        },
    )
    boost = cmake.subproject('boost', required: true, options: opt)
    boost_asio = boost.dependency('boost_asio').as_system()
    boost_beast = boost.dependency('boost_beast').as_system()
    boost_callable_traits = boost.dependency('boost_callable_traits').as_system()
    boost_process = boost.dependency('boost_process').as_system()
    boost_system = boost.dependency('boost_system').as_system()
    virtual_media_dependencies += [
        boost_asio,
        boost_beast,
        boost_callable_traits,
        boost_process,
        boost_system,
    ]
endif

nlohmann_json = dependency('nlohmann_json', include_type: 'system')
virtual_media_dependencies += nlohmann_json

phosphor_logging = dependency('phosphor-logging', required: false)
if not phosphor_logging.found()
    phosphor_logging_proj = subproject('phosphor-logging', required: true)
    phosphor_logging = phosphor_logging_proj.get_variable(
        'phosphor_logging_dep',
    )
    phosphor_logging = phosphor_logging.as_system('system')
endif
virtual_media_dependencies += phosphor_logging

sdbusplus = dependency('sdbusplus', required: false)
if not sdbusplus.found()
    sdbusplus_proj = subproject('sdbusplus', required: true)
    sdbusplus = sdbusplus_proj.get_variable('sdbusplus_dep')
    sdbusplus = sdbusplus.as_system('system')
endif
virtual_media_dependencies += sdbusplus

systemd_system_unit_dir = systemd.get_variable(
    pkgconfig: 'systemdsystemunitdir',
)

conf_data = configuration_data()

conf_data.set('MESON_INSTALL_PREFIX', get_option('prefix'))

configure_file(
    input: 'xyz.openbmc_project.VirtualMedia.service',
    output: 'xyz.openbmc_project.VirtualMedia.service',
    install_dir: systemd_system_unit_dir,
    install: true,
    configuration: conf_data,
)

install_data(
    'config/config.json',
    install_dir: '/usr/share/virtual-media',
)

srcfiles_app = [
    'src/state/activating_state.cpp',
    'src/utils/impl/child.cpp',
    'src/utils/impl/file_printer.cpp',
    'src/utils/impl/gadget_dirs.cpp',
    'src/utils/impl/pipe_reader.cpp',
    'src/utils/impl/stream_descriptor.cpp',
    'src/utils/impl/udev.cpp',
    'src/main.cpp',
    'src/resources.cpp',
]

bindir = get_option('prefix') + '/' + get_option('libexecdir')

executable(
    'virtual-media',
    srcfiles_app,
    dependencies: virtual_media_dependencies,
    include_directories: incdir,
    install: true,
    install_dir: bindir,
)

if (get_option('tests').enabled())
    subdir('tests')
endif
