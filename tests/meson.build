gtest_dep = dependency(
    'gtest_main',
    version: '>=1.11.0',
    fallback: ['gtest', 'gtest_main_dep'],
    main: true,
    required: true)

gmock_dep = dependency(
    'gmock',
    version: '>=1.11.0',
    fallback: ['gtest', 'gmock_dep'],
    required: true)

gmock_global_dep = dependency(
    'gmock-global',
    version: '>=1.0.2',
    fallback: ['gmock-global', 'gmock_global_dep'],
    disabler: true,
    required: true)

configs = [
    'virtual-media-missing-nbd.json',
    'virtual-media-nbd15.json',
    'virtual-media-nbd16.json',
    'virtual-media.json',
]

copy = find_program('res/copy_to_tmp.py')

foreach c : configs
    run_command(copy, join_paths('res', c), c, check: true)
endforeach

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
            gmock_global_dep,
            nlohmann_json,
            sdbusplus,
            udev,
            udev_lib_dep,
            phosphor_logging
        ],
        include_directories: ['../src', 'src'],
        cpp_args : '-DINJECT_MOCKS'
    ),
    timeout: 120,
)
