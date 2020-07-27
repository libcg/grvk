mantle_src = [
  'mantle_cmd_buf.c',
  'mantle_cmd_buf_man.c',
  'mantle_descriptor_set.c',
  'mantle_init_device.c',
  'mantle_image_view.c',
  'mantle_query_sync.c',
  'mantle_queue.c',
  'mantle_memory_man.c',
  'mantle_object_man.c',
  'mantle_shader_pipeline.c',
  'mantle_state_object.c',
  'mantle_wsi.c',
  'stub.c',
  'util.c',
  'vulkan_loader.c',
]

mantle_dll = shared_library('mantle'+dll_variant, mantle_src,
  name_prefix         : '',
  dependencies        : [ lib_vulkan, amdilc_dep ],
  include_directories : grvk_include_path,
  install             : true,
  objects             : 'mantle' + dll_variant + '.def',
  vs_module_defs      : 'mantle' + dll_variant + '.def')

mantle_dep = declare_dependency(
  link_with           : [ mantle_dll ],
  include_directories : [ grvk_include_path ])
