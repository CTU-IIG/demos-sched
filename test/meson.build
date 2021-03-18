configure_file(copy : true, input : 'testlib', output : 'testlib')

tests = '''
0010-help.t
0020-basic.t
0100-api.t
0200-config.t
'''.split()

foreach t: tests
	test(t, find_program(t), protocol : 'tap', workdir : meson.current_build_dir(), is_parallel: false)
endforeach
