tests = '''
0010-help.bats
0020-basic.bats
0100-api.bats
0200-config.bats
0210-config_validation.bats
0220-test-configs.bats
0300-schedule.bats
'''.split()

bats = find_program('bats', required: false, disabler: true)

foreach t: tests
	test(t, bats, args: [ t ], protocol: 'tap', workdir: meson.current_source_dir(), is_parallel: false)
endforeach
