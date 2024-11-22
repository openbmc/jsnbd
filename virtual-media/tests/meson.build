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
        ['src/dummy_test.cpp'],
        dependencies: virtual_media_dependencies
        + [
            gmock_dep,
            gtest_dep,
        ],
        include_directories: ['../src', 'src'],
    ),
    timeout: 120,
)
