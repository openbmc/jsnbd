gtest_dep = dependency(
    'gtest_main',
    version: '>=1.11.0',
    fallback: ['gtest', 'gtest_main_dep'],
    main: true,
    required: true,
    include_type: 'system',
)

gmock_dep = dependency(
    'gmock',
    version: '>=1.11.0',
    fallback: ['gtest', 'gmock_dep'],
    required: true,
    include_type: 'system',
)

test(
    'virtual-media-ut',
    executable(
        'virtual-media-ut',
        [
            'src/configuration_test.cpp',
            'src/system_test.cpp',
        ],
        dependencies: [
            boost,
            gmock_dep,
            gtest_dep,
            nlohmann_json,
            sdbusplus,
            udev,
            udev_lib_dep,
            phosphor_logging
        ],
        include_directories: ['../src', 'src'],
        cpp_args : ['-DINJECT_MOCKS', '-DTEST_DATA_DIR="' + meson.current_source_dir() + '/res"']
    ),
    timeout: 120,
)
