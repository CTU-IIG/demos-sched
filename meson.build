project('spdlog', 'cpp',
	version: '1.8.2',
	license: 'MIT')

add_project_arguments(['-w'], language : ['c', 'cpp'])
add_project_arguments(['-Wno-effc++'], language : 'cpp')

inc = include_directories('include')

thread_dep = dependency('threads')

spdlog_dependencies = [thread_dep]
spdlog_compile_args = []


if get_option('external_fmt')
    fmt_dep = dependency('fmt')
    spdlog_dependencies += fmt_dep
    spdlog_compile_args += '-DSPDLOG_FMT_EXTERNAL'
endif

if get_option('compile_library')
    spdlog_compile_args += '-DSPDLOG_COMPILED_LIB'
    subdir('src')

    spdlog_dep = declare_dependency(include_directories : inc, dependencies : spdlog_dependencies, link_with : spdlog_lib, compile_args : spdlog_compile_args)
else
    spdlog_dep = declare_dependency(include_directories : inc, dependencies : spdlog_dependencies, compile_args : spdlog_compile_args)
endif