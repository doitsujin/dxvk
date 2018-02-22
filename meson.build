project('dxvk', ['c', 'cpp'], default_options : ['cpp_std=c++1z'])

cpu_family = target_machine.cpu_family()

dxvk_compiler = meson.get_compiler('cpp')
dxvk_include_path = include_directories('./include')

if (cpu_family == 'x86_64')
  dxvk_library_path = meson.source_root() + '/lib'
else
  dxvk_library_path = meson.source_root() + '/lib32'
endif

lib_vulkan = dxvk_compiler.find_library('vulkan-1', dirs : dxvk_library_path)

lib_spirvtools     = dxvk_compiler.find_library('libSPIRV-Tools',     dirs : dxvk_library_path)
lib_spirvtools_opt = dxvk_compiler.find_library('libSPIRV-Tools-opt', dirs : dxvk_library_path)

lib_d3d11          = dxvk_compiler.find_library('d3d11')
lib_dxgi           = dxvk_compiler.find_library('dxgi')
lib_d3dcompiler_47 = dxvk_compiler.find_library('d3dcompiler_47')

glsl_compiler = find_program('glslangValidator')
glsl_generator = generator(glsl_compiler,
  output    : [ '@BASENAME@.h' ],
  arguments : [ '-V', '--vn', '@BASENAME@', '@INPUT@', '-o', '@OUTPUT@' ])

subdir('src')
subdir('tests')
subdir('wine_utils')
