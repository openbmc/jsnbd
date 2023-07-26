gtest_dep = dependency(
    'gtest',
    version: '>=1.11.0',
    main: true,
    required: true
)

gmock_dep = dependency(
    'gmock',
    version: '>=1.11.0',
    required: true
)

configs = [
    'virtual-media-missing-nbd.json',
    'virtual-media-nbd15.json',
    'virtual-media-nbd16.json',
    'virtual-media.json',
]

ut = executable(
    'virtual-media-ut',
    [
        'src/configuration_test.cpp',
        #'src/system_test.cpp',
    ],
    dependencies: [
        boost,
        gmock_dep,
        gtest_dep,
        sdbusplus,
        udev,
    ],
    include_directories: ['../src', 'src'],
)

test(
    'virtual-media-ut',
    ut,
)
