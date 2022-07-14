project(
    'virtual-media',
    'cpp',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++23',
        'cpp_rtti=false',
        'b_lto=true',
        'b_ndebug=if-release',
    ],
    meson_version: '>=1.1.1',
    license: 'Apache-2.0',
    version: '0.1',
)

if get_option('cpp_std') != 'c++23'
    error('This project requires c++23 support')
endif

cxx = meson.get_compiler('cpp')

build = get_option('buildtype')
optimization = get_option('optimization')
summary('Build Type',build, section : 'Build Info')
summary('Optimization',optimization, section : 'Build Info')

incdir = include_directories('src')

add_project_arguments(
    cxx.get_supported_arguments([
        '-Wold-style-cast',
        '-Wcast-align',
        '-Wunused',
        '-Wconversion',
        '-Wsign-conversion',
        '-Wno-attributes',
    ]),
    language: 'cpp'
)

if (cxx.get_id() == 'clang' and cxx.version().version_compare('>9.0'))
add_project_arguments(
    cxx.get_supported_arguments([
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
        '-Wunused-parameter',
        '-Wcovered-switch-default',
        '-Wcomma',
        '-Wextra-semi',
        '-Wzero-as-null-pointer-constant',
        '-Wswitch-enum',
        '-Wnull-dereference',
        '-Wdouble-promotion',
        '-Wformat=2',
    ]),
    language:'cpp')
endif

if (cxx.get_id() == 'gcc' and cxx.version().version_compare('>8.0'))
    add_project_arguments(
        cxx.get_supported_arguments([
            '-Wduplicated-cond',
            '-Wduplicated-branches',
            '-Wlogical-op',
            '-Wunused-parameter',
            '-Wnull-dereference',
            '-Wdouble-promotion',
            '-Wformat=2',
        ]),
      language:'cpp')

    if (get_option('buildtype') != 'plain')
        if (get_option('b_lto') == true and get_option('optimization')!='0')
            #Reduce the binary size by removing unnecessary
            #dynamic symbol table entries

            add_project_arguments(
            cxx.get_supported_arguments([
                '-fno-fat-lto-objects',
                '-fvisibility=hidden',
                '-fvisibility-inlines-hidden'
            ]),
            language: 'cpp')

            if cxx.has_link_argument('-Wl,--exclude-libs,ALL')
                add_project_link_arguments('-Wl,--exclude-libs,ALL', language: 'cpp')
            endif
        endif
    endif
endif

virtual_media_dependencies = []

systemd = dependency('systemd', required: true)
virtual_media_dependencies += systemd

# pkg-config for udev does not contain Libs variable
udev = dependency('udev', required: true)
# this will add appopriate udev library linkage to executable.
udev_lib_dep = declare_dependency(link_args: ['-ludev'])
virtual_media_dependencies += [udev, udev_lib_dep]

nlohmann_json = dependency('nlohmann_json', version: '>=3.11.2', include_type: 'system')
virtual_media_dependencies += nlohmann_json

boost = dependency('boost', modules: ['coroutine'], required: false, include_type: 'system')
if boost.found()
    virtual_media_dependencies += boost
else
    cmake = import('cmake')
    opt = cmake.subproject_options()
    opt.add_cmake_defines({
        'BOOST_INCLUDE_LIBRARIES': 'asio;beast;callable_traits;process;system'
    })
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
        boost_system
    ]
endif

add_project_arguments(
    cxx.get_supported_arguments([
        '-DBOOST_ASIO_DISABLE_THREADS',
        '-DBOOST_ASIO_NO_DEPRECATED',
        '-DBOOST_NO_RTTI',
        '-DBOOST_NO_TYPEID',
        '-DBOOST_ALLOW_DEPRECATED_HEADERS',
        '-DBOOST_SYSTEM_NO_DEPRECATED'
    ]),
    language : 'cpp')

sdbusplus = dependency('sdbusplus', required: false)
if not sdbusplus.found()
    sdbusplus_proj = subproject('sdbusplus', required: true)
    sdbusplus = sdbusplus_proj.get_variable('sdbusplus_dep')
    sdbusplus = sdbusplus.as_system('system')
endif
virtual_media_dependencies += sdbusplus

phosphor_logging = dependency('phosphor-logging', required: false)
if not phosphor_logging.found()
    phosphor_logging_proj = subproject('phosphor-logging', required: true)
    phosphor_logging = phosphor_logging_proj.get_variable('phosphor_logging_dep')
    phosphor_logging = phosphor_logging.as_system('system')
endif
virtual_media_dependencies += phosphor_logging

systemd_system_unit_dir = systemd.get_variable(pkgconfig:
    'systemdsystemunitdir'
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

srcfiles_app = ['src/main.cpp']

bindir = get_option('prefix') + '/' + get_option('libexecdir')

if get_option('legacy-mode').enabled()
    add_project_arguments('-DLEGACY_MODE_ENABLED', language: 'cpp')
endif

if get_option('verbose-nbdkit-logs').enabled()
    add_project_arguments('-DVM_VERBOSE_NBDKIT_LOGS', language: 'cpp')
endif

executable('virtual-media',
           srcfiles_app,
           dependencies: virtual_media_dependencies,
           include_directories: incdir,
           install: true,
           install_dir: bindir)

#Tests are placed in the tests folder, with it's own meson.build
if (get_option('tests').enabled())
    subdir('tests')
endif
