
#libev_dep = dependency('ev') # why this doesn't work? because ev doesn't provide pkgconfig .pc file
#libev_dep = meson.get_compiler('cpp').find_library('ev')

incdir = include_directories('../include')

executable('demos_sch',
           ['demos_sch.cpp','process.cpp','classes.cpp',
            'cgroup.cpp', 'timerfd.cpp','demossched.cpp'],
           include_directories : incdir,
           dependencies : libev_dep)
executable('infinite_proc', 'infinite_proc.cpp')