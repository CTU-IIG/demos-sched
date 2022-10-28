tests = '''
0010-help.t
0020-basic.t
0100-api.t
0200-config.t
0210-config_validation.t
0220-test-configs.t
0300-schedule.t
'''.split()

bats = find_program('bats', required: false)

foreach t: tests
	test(t, bats, args: [ t ], protocol: 'tap', workdir: meson.current_source_dir(), is_parallel: false)
endforeach
