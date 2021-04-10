amdilc_src = [
  'amdilc.c',
  'amdilc_compiler.c',
  'amdilc_decoder.c',
  'amdilc_dump.c',
  'amdilc_spirv.c'
]

amdilc_lib = static_library('amdilc', amdilc_src,
  dependencies        : [ logger_dep ],
  include_directories : [ grvk_include_path ],
  override_options    : [ 'c_std=' + grvk_c_std ])

amdilc_dep = declare_dependency(
  link_with           : [ amdilc_lib ],
  include_directories : [ grvk_include_path, include_directories('.') ])
