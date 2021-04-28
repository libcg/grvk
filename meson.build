project('grvk', ['c', 'cpp'], version : '0.4.0', meson_version : '>= 0.46')

cpu_family = target_machine.cpu_family()

grvk_compiler = meson.get_compiler('cpp')
grvk_c_std    = 'c99'
grvk_msvc     = grvk_compiler.get_id() == 'msvc'

grvk_include_path = include_directories('./include')

if (cpu_family == 'x86_64')
  dll_variant = '64'
  grvk_library_path = meson.source_root() + '/lib'
else
  dll_variant = '32'
  grvk_library_path = meson.source_root() + '/lib32'
endif

lib_vulkan = grvk_compiler.find_library('vulkan-1', dirs : grvk_library_path)

subdir('src')
subdir('test')
