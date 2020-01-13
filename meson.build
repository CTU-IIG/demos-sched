project('demos-sched', 'cpp', default_options : 'cpp_std=c++14')
add_global_arguments(['-Wall','-O2'], language : 'cpp')

#ev_dep = dependency('ev')
ev_dep = meson.get_compiler('cpp').find_library('ev')

executable('demos_sch', 'src/main.cpp', dependencies : ev_dep)
executable('infinite_proc', 'src/infinite_proc.cpp')



