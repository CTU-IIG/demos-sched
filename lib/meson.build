incdir = include_directories('./')

libdemos_sch = library(
  'demos-sch', [ 'demos-sch.c' ],
  c_args: (get_option('lib_verbose') ? ['-DDEMOS_LIB_VERBOSE'] : []),
)

executable('demos-signal-completion', [ 'demos-signal-completion.c' ])

libdemos_sch_dep = declare_dependency(
  link_with : libdemos_sch,
  include_directories : incdir,
)

pkg = import('pkgconfig')
pkg.generate(libdemos_sch)
