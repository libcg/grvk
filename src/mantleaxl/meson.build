mantleaxl_src = [
  'main.c',
  'mantleaxl_border_color_palette_ext.c',
  'mantleaxl_lib_version.c',
  'stub.c'
]

mantleaxl_def = 'mantleaxl' + dll_variant + '.def'

mantleaxl_dll = shared_library('mantleaxl' + dll_variant, mantleaxl_src,
  grvk_version,
  name_prefix         : '',
  dependencies        : [ lib_vulkan, mantle_dep, amdilc_dep, logger_dep ],
  include_directories : grvk_include_path,
  install             : true,
  objects             : not grvk_msvc ? mantleaxl_def : [],
  vs_module_defs      : mantleaxl_def,
  override_options    : [ 'c_std=' + grvk_c_std ])

mantleaxl_dep = declare_dependency(
  link_with           : [ mantleaxl_dll ],
  include_directories : [ grvk_include_path ])
