
#libev_dep = dependency('ev') # why this doesn't work? because ev doesn't provide pkgconfig .pc file
#libev_dep = meson.get_compiler('cpp').find_library('ev')

incdir = include_directories('../include')

cxx = meson.get_compiler('cpp')

executable('demos-sched',
           ['demos-sched.cpp','process.cpp','partition.cpp',
            'slice.cpp','window.cpp','majorframe.cpp',
            'cgroup.cpp', 'timerfd.cpp', 'evfd.cpp'],
           include_directories : incdir,
	   cpp_args : cxx.get_supported_arguments(['-Wsuggest-attribute=const']),
           dependencies : [libev_dep, yaml_cpp_dep] )

executable('eventfd-test',
           ['eventfd_test.cpp','evfd.cpp'],
           include_directories : incdir,
           dependencies : libev_dep )

executable('api-test',
           ['api_test.cpp','demos-sch.cpp'],
           include_directories : incdir,
           dependencies : libev_dep )

# executable('cgroup-test',
#            ['cgroup_test.cpp', 'cgroup.cpp'],
#            include_directories : incdir,
#            dependencies : libev_dep )

# executable('partition-test',
#            ['partition_test.cpp', 'cgroup.cpp',
#             'process.cpp', 'partition.cpp'],
#            include_directories : incdir,
#            dependencies : libev_dep )

# executable('slice-test',
#            ['slice_test.cpp', 'cgroup.cpp',
#             'process.cpp', 'partition.cpp', 'slice.cpp',
#             'timerfd.cpp'],
#            include_directories : incdir,
#            dependencies : libev_dep )

# executable('majorframe-test',
#            ['majorframe_test.cpp','process.cpp','partition.cpp',
#             'slice.cpp','window.cpp','majorframe.cpp',
#             'cgroup.cpp', 'timerfd.cpp'],
#            include_directories : incdir,
#            dependencies : libev_dep)

executable('infinite_proc', 'infinite_proc.cpp')

# executable('cgroup_notif_test', 'cgroup_notif_test.cpp',
#            include_directories : incdir,
#            dependencies : libev_dep)

test('timer_fd',
     executable('timerfd_test',
		['timerfd_test.cpp', 'timerfd.cpp'],
		include_directories : incdir,
		dependencies : libev_dep),
    )
executable('yaml-demo',
           ['yaml_demo.cpp'],
           dependencies : yaml_cpp_dep)
        
