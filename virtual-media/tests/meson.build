gtest_dep = dependency(
    'gtest',
    version: '>=1.11.0',
    fallback: ['gtest', 'gtest_dep'],
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
            'src/environments/dbus_environment.cpp',
            'src/fakes/child_fake.cpp',
            'src/fakes/file_printer_fake.cpp',
            'src/fakes/gadget_dirs_fake.cpp',
            'src/fakes/pipe_reader_fake.cpp',
            'src/fakes/stream_descriptor_fake.cpp',
            'src/fakes/udev_fake.cpp',
            'src/configuration_test.cpp',
            'src/events_test.cpp',
            'src/initial_state_test.cpp',
            'src/main.cpp',
            'src/system_test.cpp',
        ],
        dependencies: virtual_media_dependencies
        + [
            gmock_dep,
            gtest_dep,
        ],
        include_directories: ['../src', 'src'],
    ),
    timeout: 120,
)
