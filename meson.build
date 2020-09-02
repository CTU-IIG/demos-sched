project('demos-sched', ['cpp', 'c'],
	default_options : [
	  'cpp_std=c++14',
	  'default_library=static',
	])
add_global_arguments(['-Weffc++'], language : 'cpp')

cpp = meson.get_compiler('cpp')

libev_proj = subproject('libev', required : false)
if libev_proj.found()
  libev_dep = libev_proj.get_variable('libev_dep')
else
  libev_dep = cpp.find_library('ev')
  if not cpp.has_header_symbol('ev.h', 'EV_EXCEPTION', dependencies : libev_dep)
    error('Unpatched libev detected')
  endif
endif
yaml_cpp_proj = subproject('yaml-cpp', required : false)
if yaml_cpp_proj.found()
  # Use version from the submodule (if initialized)
  yaml_cpp_dep = yaml_cpp_proj.get_variable('yaml_cpp_dep')
else
  # Use system-provided version
  yaml_cpp_dep = dependency('yaml-cpp')
endif

if meson.version().version_compare('>=0.52')
  # By default, gcc does not report warnings in system header. Treat
  # libev as system header to not see warnings from there.
  if libev_proj.found()
    libev_dep = libev_dep.as_system()
  endif
  yaml_cpp_dep = yaml_cpp_dep.as_system()
endif

subdir('src')
subdir('test')
