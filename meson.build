project('dxvk', ['c', 'cpp'])

dxvk_compiler = meson.get_compiler('cpp')
dxvk_library_path = meson.source_root() + '/lib'
dxvk_include_path = include_directories('./include')

lib_vulkan = dxvk_compiler.find_library('vulkan-1', dirs : dxvk_library_path)
lib_sdl2   = dxvk_compiler.find_library('SDL2',     dirs : dxvk_library_path)

subdir('src')
subdir('tests')