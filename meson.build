project('demos-sched', 'cpp',
	default_options : [
	  'cpp_std=c++14',
	  'default_library=static',
	])

libev_proj = subproject('libev')
libev_dep = libev_proj.get_variable('libev_dep')
yaml_cpp_proj = subproject('yaml-cpp')
yaml_cpp_dep = yaml_cpp_proj.get_variable('yaml_cpp_dep')

if meson.version().version_compare('>=0.52')
  # By default, gcc does not report warnings in system header. Treat
  # libev as system header to not see warnings from there.
  libev_dep = libev_dep.as_system()
  yaml_cpp_dep = yaml_cpp_dep.as_system()
endif

subdir('src')
subdir('test')
