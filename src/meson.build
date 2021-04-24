cxx = meson.get_compiler('cpp')

executable('demos-sched',
	['main.cpp', 'memory_tracker.cpp',
	 'process.cpp', 'partition.cpp',
	 'slice.cpp', 'window.cpp', 'majorframe.cpp',
	 'cgroup.cpp', 'cgroup_setup.cpp', 'timerfd.cpp', 'evfd.cpp',
	 'config.cpp', 'cpuset.c', 'log.cpp'],
	cpp_args : cxx.get_supported_arguments(['-Wsuggest-attribute=const']) +
		[ '-DHAVE_DECL_CPU_ALLOC' ], # See cpuset.h
	dependencies : [libev_dep, yaml_cpp_dep, spdlog_dep],
	install : true)

test('evfd', executable('evfd_tests', ['evfd.tests.cpp','evfd.cpp'], dependencies : libev_dep))
test('timerfd', executable('timerfd_tests', ['timerfd.tests.cpp', 'timerfd.cpp'], dependencies : libev_dep))


subdir('tests')

configure_file(
	output : 'config_meson.h',
	configuration : configuration_data({
		'MAX_CPUS' : get_option('max_cpus'),
		'LOG_ALLOCATIONS' : get_option('log_allocations'),
		'SPDLOG_ACTIVE_LEVEL': (get_option('trace_logs')
		    ? 'SPDLOG_LEVEL_TRACE' : 'SPDLOG_LEVEL_INFO'),
	}),
)
