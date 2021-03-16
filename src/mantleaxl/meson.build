mantleaxl_src = [
  'stub.c'
]

mantleaxl_def = 'mantleaxl' + dll_variant + '.def'

mantleaxl_dll = shared_library('mantleaxl' + dll_variant, mantleaxl_src,
  name_prefix         : '',
  dependencies        : [ lib_vulkan, logger_dep ],
  include_directories : grvk_include_path,
  install             : true,
  objects             : not grvk_msvc ? mantleaxl_def : [],
  vs_module_defs      : mantleaxl_def,
  override_options    : [ 'c_std=' + grvk_c_std ])

mantleaxl_dep = declare_dependency(
  link_with           : [ mantleaxl_dll ],
  include_directories : [ grvk_include_path ])
