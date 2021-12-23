project(
    'virtual-media',
    'cpp',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++20',
        'cpp_rtti=false',
    ],
    meson_version: '>=0.55.0',
    license: 'Apache-2.0',
    version: '0.1',
)

if get_option('cpp_std') != 'c++20'
    error('This project requires c++20 support')
endif

cxx = meson.get_compiler('cpp')

build = get_option('buildtype')
optimization = get_option('optimization')
summary('Build Type',build, section : 'Build Info')
summary('Optimization',optimization, section : 'Build Info')

#remove debug information for minsize buildtype
if(get_option('buildtype') == 'minsize')
  add_project_arguments('-DNDEBUG', language : 'cpp')
endif

#Disable lto when compiling with no optimization
if(get_option('optimization') == '0')
  add_project_arguments('-fno-lto', language: 'cpp')
  message('Disabling lto & its supported features as optimization is disabled')
endif

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

security_flags = [
'-fstack-protector-strong',
'-fPIE',
'-fPIC',
'-D_FORTIFY_SOURCE=2',
'-Wformat',
'-Wformat-security'
]

## Add security flags for builds of type 'release','debugoptimized' and 'minsize'

if not (get_option('buildtype') == 'plain' or get_option('buildtype').startswith('debug'))
  add_project_arguments(
   cxx.get_supported_arguments([
    security_flags
  ]),
  language: 'cpp')
endif

systemd = dependency('systemd', required: true)

# pkg-config for udev does not contain Libs variable
udev = dependency('udev', required: true)
# this will add appopriate udev library linkage to executable.
udev_lib_dep = declare_dependency(link_args: ['-ludev'])

if cxx.has_header('nlohmann/json.hpp')
    nlohmann_json = declare_dependency()
else
    nlohmann_json_proj = subproject('nlohmann', required: true)
    nlohmann_json = nlohmann_json_proj.get_variable('nlohmann_json_dep')
    nlohmann_json = nlohmann_json.as_system('system')
endif

boost = dependency('boost',
                   modules: ['coroutine', 'iostreams', 'filesystem',
			     'program_options', 'regex', 'system'],
                   version: '>=1.80',
                   fallback : ['boost', 'boost_dep'],
                   static: false,
                   required: true)

sdbusplus = dependency('sdbusplus', required: false)
if not sdbusplus.found()
  sdbusplus_proj = subproject('sdbusplus', required: true)
  sdbusplus = sdbusplus_proj.get_variable('sdbusplus_dep')
  sdbusplus = sdbusplus.as_system('system')
endif


add_project_arguments(
cxx.get_supported_arguments([
'-DBOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT',
'-DBOOST_ASIO_DISABLE_THREADS',
'-DBOOST_NO_RTTI',
'-DBOOST_NO_TYPEID',
'-DBOOST_ALLOW_DEPRECATED_HEADERS'
]),
language : 'cpp')

systemd_system_unit_dir = systemd.get_pkgconfig_variable(
    'systemdsystemunitdir'
)


configure_file(input: 'xyz.openbmc_project.VirtualMedia.service',
               output: 'xyz.openbmc_project.VirtualMedia.service',
               install_dir: systemd_system_unit_dir,
               copy: true,
               install: true)

configure_file(input : 'tmpfiles.d/virtual-media.conf',
               output : 'virtual-media.conf',
               copy : true,
               install_dir: '/etc/tmpfiles.d',
               install : true)

configure_file(input : 'config/config.json',
               output : 'config.json',
               copy : true,
               install_dir: '/usr/share/virtual-media/',
               install : true)

srcfiles_app = [ 'src/main.cpp',
               ]

bindir = get_option('prefix') + '/' +get_option('libexecdir')

if get_option('legacy-mode').enabled()
  add_project_arguments(
    cxx.get_supported_arguments([
      '-DLEGACY_MODE_ENABLED=ON',]),
    language: 'cpp')
endif

executable('virtual-media',
           srcfiles_app,
           dependencies: [ systemd, boost, udev, udev_lib_dep,
                           sdbusplus, nlohmann_json,
           ],
           include_directories: incdir,
           install: true,
           install_dir:bindir)

#Tests are placed in the tests folder, with it's own meson.build
if (get_option('tests').enabled())
    subdir('tests')
endif
