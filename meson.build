project('dxvk', ['c', 'cpp'])

cpu_family = target_machine.cpu_family()

dxvk_compiler = meson.get_compiler('cpp')
dxvk_cpp_std  = 'c++1z'

# MSVC
if dxvk_compiler.get_id() == 'msvc'
  add_global_arguments('-DNOMINMAX', language : 'cpp')
  dxvk_cpp_std='c++latest'
endif

dxvk_include_path = include_directories('./include')

dxvk_library_path = meson.source_root()
dxvk_library_path += (cpu_family == 'x86_64') ? '/lib' : '/lib32'

dxvk_wine_builtin = meson.get_cross_property('wine_builtin', false)

# 
if not dxvk_wine_builtin
  lib_vulkan  = dxvk_compiler.find_library('vulkan-1', dirs : dxvk_library_path)
endif

# GLSL generator
glsl_compiler = find_program('glslangValidator')
glsl_generator = generator(glsl_compiler,
  output    : [ '@BASENAME@.h' ],
  arguments : [ '-V', '--vn', '@BASENAME@', '@INPUT@', '-o', '@OUTPUT@' ])

# WINE
dll_suffix = ''
dll_redirect_suffix = (get_option('enable_dll_redirects')) ? '-vk' : ''
if dxvk_wine_builtin
  # WINE libs are *.dll.so
  dll_suffix = dll_redirect_suffix + '.dll'

  wine_spec_path = meson.source_root() + '/linux/'
  lib_vulkan = declare_dependency(link_args: [ '-L' + wine_spec_path, '-lvulkan-1', '-lpthread' ])
  if get_option('enable_tests')
    lib_d3dcompiler_47 = declare_dependency(link_args: [ '-L' + wine_spec_path, '-ld3dcompiler_47' ])
  endif
endif

subdir('src')

# test executables
if get_option('enable_tests')
  lib_d3d11   = dxvk_compiler.find_library('d3d11')
  lib_dxgi    = dxvk_compiler.find_library('dxgi')

  if dxvk_compiler.get_id() != 'msvc'
    lib_d3dcompiler_47 = dxvk_compiler.find_library('d3dcompiler_47')
  endif

  subdir('tests')
endif

if dxvk_compiler.get_id() != 'msvc'
  subdir('wine_utils')
endif
