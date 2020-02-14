
#libev_dep = dependency('ev') # why this doesn't work? because ev doesn't provide pkgconfig .pc file
#libev_dep = meson.get_compiler('cpp').find_library('ev')

incdir = include_directories('../include')

executable('demos-sched',
           ['demos-sched.cpp','process.cpp','partition.cpp',
            'slice.cpp','window.cpp','majorframe.cpp',
            'cgroup.cpp', 'timerfd.cpp',
            'config_parser.cpp'],
           include_directories : incdir,
           dependencies : [libev_dep, dependency('yaml-cpp')] )

executable('cgroup-test',
           ['cgroup_test.cpp', 'cgroup.cpp'],
           include_directories : incdir,
           dependencies : libev_dep )

executable('partition-test',
           ['partition_test.cpp', 'cgroup.cpp',
            'process.cpp', 'partition.cpp'],
           include_directories : incdir,
           dependencies : libev_dep )

executable('slice-test',
           ['slice_test.cpp', 'cgroup.cpp',
            'process.cpp', 'partition.cpp', 'slice.cpp',
            'timerfd.cpp'],
           include_directories : incdir,
           dependencies : libev_dep )

executable('majorframe-test',
           ['majorframe_test.cpp','process.cpp','partition.cpp',
            'slice.cpp','window.cpp','majorframe.cpp',
            'cgroup.cpp', 'timerfd.cpp'],
           include_directories : incdir,
           dependencies : libev_dep)

executable('infinite_proc', 'infinite_proc.cpp')

executable('cgroup_notif_test', 'cgroup_notif_test.cpp',
           include_directories : incdir,
           dependencies : libev_dep)

executable('timerfd_test',
           ['timerfd_test.cpp', 'timerfd.cpp'],
           include_directories : incdir,
           dependencies : libev_dep)

#executable('yaml-demo',
#           ['yaml_demo.cpp'],
#           dependencies : dependency('yaml-cpp'))
        
