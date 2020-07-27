amdilc_src = [
  'amdilc.c',
  'amdilc_decoder.c',
  'amdilc_dump.c',
  'util.c',
]

amdilc_lib = static_library('amdilc', amdilc_src,
  include_directories : [ grvk_include_path ])

amdilc_dep = declare_dependency(
  link_with           : [ amdilc_lib ],
  include_directories : [ grvk_include_path, include_directories('.') ])
