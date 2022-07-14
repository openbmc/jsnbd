project('boost', 'cpp',
         version: '1.79',
         meson_version: '>=0.56.0')

cxx = meson.get_compiler('cpp')

# Build if not available
build_dir = join_paths(meson.current_source_dir(), 'build')
r = run_command('[','!','-d', build_dir, ']')
if r.returncode() == 0
      r = run_command('./bootstrap.sh', '--with-libraries=coroutine,filesystem,iostreams,program_options,regex,system')
      assert(r.returncode() == 0,
            'Bootstrap failed' +
            '\nstdout: ' + r.stdout().strip() +
            '\nstderr: ' + r.stderr().strip())

      r = run_command('./b2', 'install','--prefix=build')
      assert(r.returncode() == 0,
            'b2 failed' +
            '\nstdout: ' + r.stdout().strip() +
            '\nstderr: ' + r.stderr().strip())
endif

include_dir = join_paths('build', 'include')
lib_dir = join_paths(meson.current_source_dir(), 'build', 'lib')

custom_dep = declare_dependency(link_args: ['-L' + lib_dir,
					    '-Wl,-rpath-link,' + lib_dir
					   ])

boost_inc = include_directories(include_dir, is_system:true)
boost_dep = declare_dependency(
       include_directories : boost_inc,
       dependencies : [cxx.find_library('boost_context', dirs: lib_dir, static: true),
                       cxx.find_library('boost_filesystem', dirs: lib_dir, static: true),
                       cxx.find_library('boost_regex', dirs: lib_dir, static: true),
		       cxx.find_library('boost_program_options', dirs: lib_dir, static: true),
                       cxx.find_library('boost_coroutine', dirs: lib_dir, static: true),
                       cxx.find_library('boost_iostreams', dirs: lib_dir, static: true),
		       custom_dep
		      ])

meson.override_dependency('boost', boost_dep)
