project(
    'virtual-media',
    'cpp',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++23',
        'cpp_rtti=false',
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
summary('Build Type', build, section: 'Build Info')
summary('Optimization', optimization, section: 'Build Info')

#remove debug information for minsize buildtype
if (get_option('buildtype') == 'minsize')
    add_project_arguments('-DNDEBUG', language: 'cpp')
endif

#Disable lto when compiling with no optimization
if (get_option('optimization') == '0')
    add_project_arguments('-fno-lto', language: 'cpp')
    message(
        'Disabling lto & its supported features as optimization is disabled',
    )
endif

incdir = include_directories('src')

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

    if (get_option('buildtype') != 'plain')
        if (get_option('b_lto') == true and get_option('optimization') != '0')
            #Reduce the binary size by removing unnecessary
            #dynamic symbol table entries

            add_project_arguments(
                cxx.get_supported_arguments(
                    [
                        '-fno-fat-lto-objects',
                        '-fvisibility=hidden',
                        '-fvisibility-inlines-hidden',
                    ],
                ),
                language: 'cpp',
            )

            if cxx.has_link_argument('-Wl,--exclude-libs,ALL')
                add_project_link_arguments(
                    '-Wl,--exclude-libs,ALL',
                    language: 'cpp',
                )
            endif
        endif
    endif
endif

security_flags = [
    '-fstack-protector-strong',
    '-fPIE',
    '-fPIC',
    '-D_FORTIFY_SOURCE=2',
    '-Wformat',
    '-Wformat-security',
]

## Add security flags for builds of type 'release','debugoptimized' and 'minsize'

if (
    not (
        get_option('buildtype') == 'plain'
        or get_option('buildtype').startswith('debug')
    )
)
    add_project_arguments(
        cxx.get_supported_arguments([security_flags]),
        language: 'cpp',
    )
endif

systemd = dependency('systemd', required: true)

# pkg-config for udev does not contain Libs variable
udev = dependency('udev', required: true)
# this will add appopriate udev library linkage to executable.
udev_lib_dep = declare_dependency(link_args: ['-ludev'])

nlohmann_json = dependency(
    'nlohmann_json',
    version: '>=3.11.2',
    include_type: 'system',
)

boost = dependency(
    'boost',
    modules: ['coroutine'],
    required: false,
    include_type: 'system',
)
if not boost.found()
    boost_proj = subproject('boost', required: true)
    boost = boost_proj.get_variable('boost_dep')
    boost = boost.as_system('system')
endif

add_project_arguments(
    cxx.get_supported_arguments(
        [
            '-DBOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT',
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

sdbusplus = dependency('sdbusplus', required: false)
if not sdbusplus.found()
    sdbusplus_proj = subproject('sdbusplus', required: true)
    sdbusplus = sdbusplus_proj.get_variable('sdbusplus_dep')
    sdbusplus = sdbusplus.as_system('system')
endif

phosphor_logging = dependency('phosphor-logging', required: false)
if not phosphor_logging.found()
    phosphor_logging_proj = subproject('phosphor-logging', required: true)
    phosphor_logging = phosphor_logging_proj.get_variable(
        'phosphor_logging_dep',
    )
    phosphor_logging = phosphor_logging.as_system('system')
endif

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
    install_dir: '/etc/virtual-media',
)
srcfiles_app = ['src/main.cpp']

bindir = get_option('prefix') + '/' + get_option('bindir')

if get_option('legacy-mode').enabled()
    add_project_arguments(
        cxx.get_supported_arguments(
            [
                '-DLEGACY_MODE_ENABLED=ON',
            ],
        ),
        language: 'cpp',
    )
endif

if get_option('verbose-nbdkit-logs').enabled()
    add_project_arguments(
        cxx.get_supported_arguments(
            [
                '-DVM_VERBOSE_NBDKIT_LOGS=ON',
            ],
        ),
        language: 'cpp',
    )
endif

executable(
    'virtual-media',
    srcfiles_app,
    dependencies: [
        systemd,
        boost,
        udev,
        udev_lib_dep,
        sdbusplus,
        nlohmann_json,
        phosphor_logging,
    ],
    include_directories: incdir,
    install: true,
    install_dir: bindir,
)

#Tests are placed in the tests folder, with it's own meson.build
if (get_option('tests').enabled())
    subdir('tests')
endif
