
#libev_dep = dependency('ev') # why this doesn't work? because ev doesn't provide pkgconfig .pc file
#libev_dep = meson.get_compiler('cpp').find_library('ev')

cxx = meson.get_compiler('cpp')

executable('demos-sched',
           ['demos-sched.cpp','process.cpp','partition.cpp',
            'slice.cpp','window.cpp','majorframe.cpp',
            'cgroup.cpp', 'timerfd.cpp', 'evfd.cpp',
            'config.cpp', 'cpuset.c'
	   ],
	   cpp_args : cxx.get_supported_arguments(['-Wsuggest-attribute=const']) +
		      [ '-DHAVE_DECL_CPU_ALLOC' ], # See cpuset.h
           dependencies : [libev_dep, yaml_cpp_dep],
	   install : true,
	  )

executable('eventfd-test',
           ['eventfd_test.cpp','evfd.cpp'],
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
		dependencies : libev_dep),
    )
executable('yaml-demo',
           ['yaml_demo.cpp'],
           dependencies : yaml_cpp_dep)

incdir = include_directories('../include')
libdemos_sch = library('demos-sch', [ 'demos-sch.c' ],
		       include_directories : incdir)
libdemos_sch_dep = declare_dependency(
  link_with : libdemos_sch,
  include_directories : incdir,
)

executable('api-test', ['api_test.c'],
           dependencies : [ libdemos_sch_dep ])

conf_data = configuration_data({
  'MAX_CPUS' : get_option('max_cpus'),
})

configure_file(
  output : 'config.h',
  configuration : conf_data,
)
