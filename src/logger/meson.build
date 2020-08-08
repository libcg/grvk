logger_src = [
  'logger.c',
]

logger_lib = static_library('logger', logger_src,
  include_directories : [ grvk_include_path ],
  override_options    : [ 'c_std=' + grvk_c_std ])

logger_dep = declare_dependency(
  link_with           : [ logger_lib ],
  include_directories : [ grvk_include_path, include_directories('.') ])
