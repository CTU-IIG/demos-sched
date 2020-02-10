project('demos-sched', 'cpp',
	default_options : [
	  'cpp_std=c++14',
	  'default_library=static',
	])
add_global_arguments(['-Wall'], language : 'cpp')

libev_proj = subproject('libev')
libev_dep = libev_proj.get_variable('libev_dep')

subdir('src')
