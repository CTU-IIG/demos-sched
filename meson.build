project('demos-sched', 'cpp',
	default_options : [
	  'cpp_std=c++14',
	  'default_library=static',
	])
add_global_arguments(['-Wall'], language : 'cpp')

libev_proj = subproject('libev')
libev_dep = libev_proj.get_variable('libev_dep')
if meson.version().version_compare('>=0.52')
  # By default, gcc does not report warnings in system header. Treat
  # libev as system header to not see warnings from there.
  libev_dep = libev_dep.as_system()
endif

subdir('src')
