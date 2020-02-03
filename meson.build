project('demos-sched', 'cpp', default_options : 'cpp_std=c++14')
add_global_arguments(['-Wall','-O2'], language : 'cpp')

#ev_dep = dependency('ev') # why this doesn't work?
ev_dep = meson.get_compiler('cpp').find_library('ev')

incdir = include_directories('include')

executable('demos_sch',
           ['src/demos_sch.cpp','src/process.cpp','src/functions.cpp','src/classes.cpp',
            'src/cgroup.cpp'],
           include_directories : incdir,
           dependencies : ev_dep)
executable('infinite_proc', 'src/infinite_proc.cpp')



