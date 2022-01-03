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
            '../src/state/activating_state.cpp',
            '../src/resources.cpp',

            'src/mocks/child_mock.cpp',
            'src/mocks/file_printer_mock.cpp',
            'src/mocks/gadget_dirs_mock.cpp',
            'src/mocks/mounter_mock.cpp',
            'src/mocks/pipe_reader_mock.cpp',
            'src/mocks/stream_descriptor_mock.cpp',
            'src/mocks/udev_mock.cpp',

            'src/activating_state_test.cpp',
            'src/configuration_test.cpp',
            'src/events_test.cpp',
            'src/initial_state_test.cpp',
            'src/resources_test.cpp',
            'src/smb_test.cpp',
            'src/state_machine_test.cpp',
            'src/state_transitions_test.cpp',
            'src/system_test.cpp',
            'src/utils_test.cpp',
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
        cpp_args : '-DINJECT_MOCKS'
    ),
    timeout: 120,
)
