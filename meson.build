project('dxvk', ['c', 'cpp'])

cpu_family = target_machine.cpu_family()

dxvk_compiler = meson.get_compiler('cpp')
dxvk_include_path = include_directories('./include')

if (cpu_family == 'x86_64')
  dxvk_library_path = meson.source_root() + '/lib'
else
  dxvk_library_path = meson.source_root() + '/lib32'
endif

lib_vulkan = dxvk_compiler.find_library('vulkan-1', dirs : dxvk_library_path)
lib_sdl2   = dxvk_compiler.find_library('SDL2',     dirs : dxvk_library_path)

lib_d3d11          = dxvk_compiler.find_library('d3d11')
lib_dxgi           = dxvk_compiler.find_library('dxgi')
lib_d3dcompiler_47 = dxvk_compiler.find_library('d3dcompiler_47')

subdir('src')
subdir('tests')