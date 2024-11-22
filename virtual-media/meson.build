build = get_option('buildtype')
optimization = get_option('optimization')
summary('Build Type', build, section: 'Build Info')
summary('Optimization', optimization, section: 'Build Info')

incdir = include_directories('src')

virtual_media_dependencies = []

systemd = dependency('systemd', required: true)
virtual_media_dependencies += systemd

boost = dependency(
    'boost',
    required: false,
    include_type: 'system',
)
if boost.found()
    virtual_media_dependencies += boost
else
    cmake = import('cmake')
    opt = cmake.subproject_options()
    opt.add_cmake_defines({'BOOST_INCLUDE_LIBRARIES': 'asio;callable_traits;system'})
    boost = cmake.subproject('boost', required: true, options: opt)
    boost_asio = boost.dependency('boost_asio').as_system()
    boost_callable_traits = boost.dependency('boost_callable_traits').as_system()
    boost_system = boost.dependency('boost_system').as_system()
    virtual_media_dependencies += [boost_asio, boost_callable_traits, boost_system]
endif

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

srcfiles_app = ['src/main.cpp']

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
