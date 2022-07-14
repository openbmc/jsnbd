project(
    'gmock-global',
    'cpp',
    version: '1.0.2',
    meson_version: '>=0.55.0',
    license: 'MIT')

gmock_global_inc = include_directories('include')

gmock_global_dep = declare_dependency(
    include_directories: gmock_global_inc
)

meson.override_dependency('gmock-global', gmock_global_dep)
