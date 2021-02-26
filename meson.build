project('yaml_cpp', ['cpp'],
	version: '0.6.3',
	default_options : [
	  'default_library=static',
	  'buildtype=release',
	])

add_project_arguments(['-w'], language : ['c', 'cpp'])
add_project_arguments(['-Wno-effc++'], language : 'cpp')

# contrib-pattern = 'src/contrib/*.cpp'

inc_dir = include_directories('include')
yaml_cpp_lib = library('yaml-cpp',
		       [
			 'src/binary.cpp',
			 'src/convert.cpp',
			 'src/directives.cpp',
			 'src/emit.cpp',
			 'src/emitfromevents.cpp',
			 'src/emitter.cpp',
			 'src/emitterstate.cpp',
			 'src/emitterutils.cpp',
			 'src/exceptions.cpp',
			 'src/exp.cpp',
			 'src/memory.cpp',
			 'src/nodebuilder.cpp',
			 'src/node.cpp',
			 'src/node_data.cpp',
			 'src/nodeevents.cpp',
			 'src/null.cpp',
			 'src/ostream_wrapper.cpp',
			 'src/parse.cpp',
			 'src/parser.cpp',
			 'src/regex_yaml.cpp',
			 'src/scanner.cpp',
			 'src/scanscalar.cpp',
			 'src/scantag.cpp',
			 'src/scantoken.cpp',
			 'src/simplekey.cpp',
			 'src/singledocparser.cpp',
			 'src/stream.cpp',
			 'src/tag.cpp',
		       ],
		       install : true,
		       include_directories: inc_dir,
		       cpp_args : meson.get_compiler('cpp').get_supported_arguments(
			 '-Wall -Wextra -Wshadow -Weffc++ -Wno-long-long -pedantic -pedantic-errors'.split()
		       ),
		       override_options: ['cpp_std=c++11'],
		      )

yaml_cpp_dep = declare_dependency(
  include_directories : inc_dir,
  link_with : yaml_cpp_lib,
)
