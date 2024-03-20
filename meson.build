project('dxvk', ['c', 'cpp'], version : 'v2.3.1', meson_version : '>= 0.58', default_options : [ 'cpp_std=c++17', 'warning_level=2' ])

cpu_family = target_machine.cpu_family()
platform   = target_machine.system()
fs = import('fs')

cpp = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
dxvk_is_msvc = cpp.get_argument_syntax() == 'msvc'

compiler_args = [
  '-msse',
  '-msse2',
  '-msse3',
  '-mfpmath=sse',
  '-Wimplicit-fallthrough',
  # gcc
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',
  '-Wno-cast-function-type', # Needed for GetProcAddress.
  # clang
  '-Wno-unused-private-field',
  '-Wno-microsoft-exception-spec',
  '-Wno-extern-c-compat',
  '-Wno-unused-const-variable',
  '-Wno-missing-braces',
]

link_args = []

if get_option('build_id')
  link_args += [
    '-Wl,--build-id',
  ]
endif

dxvk_include_dirs = ['./include']
if fs.is_dir('./include/vulkan/include')
  dxvk_include_dirs += ['./include/vulkan/include']
elif not cpp.check_header('vulkan/vulkan.h')
  error('Missing Vulkan-Headers')
endif
if fs.is_dir('./include/spirv/include')
  dxvk_include_dirs += ['./include/spirv/include']
elif not cpp.check_header('spirv/unified1/spirv.hpp')
  error('Missing SPIRV-Headers')
endif

dep_displayinfo = dependency(
  'libdisplay-info',
  version: ['>= 0.0.0', '< 0.2.0'],
  fallback: ['libdisplay-info', 'di_dep'],
  default_options: ['default_library=static'],
)

if platform == 'windows'
  compiler_args += [
    '-DNOMINMAX',
    '-D_WIN32_WINNT=0xa00',
  ]

  if not dxvk_is_msvc
    link_args += [
      '-static',
      '-static-libgcc',
      '-static-libstdc++',
      # We need to set the section alignment for debug symbols to
      # work properly as well as avoiding a memcpy from the Wine loader.
      '-Wl,--file-alignment=4096',
    ]

    # Wine's built-in back traces only work with dwarf4 symbols
    if get_option('debug')
      compiler_args += [
        '-gdwarf-4',
      ]
    endif

    # Enable stdcall fixup on 32-bit
    if cpu_family == 'x86'
      link_args += [
        '-Wl,--enable-stdcall-fixup',
        '-Wl,--kill-at',
      ]
    endif
  else
    link_args += [
      '/FILEALIGN:4096',
    ]
  endif

  lib_d3d9    = cpp.find_library('d3d9')
  lib_d3d11   = cpp.find_library('d3d11')
  lib_dxgi    = cpp.find_library('dxgi')

  if dxvk_is_msvc
    lib_d3dcompiler_47 = cpp.find_library('d3dcompiler')
  else
    lib_d3dcompiler_47 = cpp.find_library('d3dcompiler_47')
  endif

  if dxvk_is_msvc
    res_ext = '.res'
    wrc = find_program('rc')
    wrc_generator = generator(wrc,
      output    : [ '@BASENAME@' + res_ext ],
      arguments : [ '/fo', '@OUTPUT@', '@INPUT@' ],
    )
  else
    res_ext = '.o'
    wrc = find_program('windres')
    wrc_generator = generator(wrc,
      output    : [ '@BASENAME@' + res_ext ],
      arguments : [ '-i', '@INPUT@', '-o', '@OUTPUT@' ],
    )
  endif

  dxvk_wsi = 'win32'
  dxvk_name_prefix = ''
  compiler_args += ['-DDXVK_WSI_WIN32']
else
  wrc           = find_program('touch')
  wrc_generator = generator(wrc, output : [ '@BASENAME@_ignored.h' ], arguments : [ '@OUTPUT@' ] )

  dxvk_include_dirs += [
    './include/native',
    './include/native/windows',
    './include/native/directx'
  ]

  dxvk_wsi = get_option('dxvk_native_wsi')
  
  if dxvk_wsi == 'sdl2'
    lib_sdl2 = cpp.find_library('SDL2')
    compiler_args += ['-DDXVK_WSI_SDL2']
  elif dxvk_wsi == 'glfw'
    lib_glfw = cpp.find_library('glfw')
    compiler_args += ['-DDXVK_WSI_GLFW']
  endif
  
  dxvk_name_prefix = 'libdxvk_'

  link_args += [
    '-static-libgcc',
    '-static-libstdc++',
  ]
endif

dxvk_include_path = include_directories(dxvk_include_dirs)

add_project_arguments(cpp.get_supported_arguments(compiler_args), language: 'cpp')
add_project_arguments(cc.get_supported_arguments(compiler_args), language: 'c')
add_project_link_arguments(cpp.get_supported_link_arguments(link_args), language: 'cpp')
add_project_link_arguments(cc.get_supported_link_arguments(link_args), language: 'c')

exe_ext = ''
dll_ext = ''
def_spec_ext = '.def'

glsl_compiler = find_program('glslang', 'glslangValidator')
glsl_args = [
  '--quiet',
  '--target-env', 'vulkan1.2',
  '--vn', '@BASENAME@',
  '--depfile', '@DEPFILE@',
  '@INPUT@',
  '-o', '@OUTPUT@',
]
glsl_generator = generator(
  glsl_compiler,
  output    : [ '@BASENAME@.h' ],
  depfile   : '@BASENAME@.h.d',
  arguments : glsl_args,
)

dxvk_version = vcs_tag(
  command: ['git', 'describe', '--dirty=+'],
  input:  'version.h.in',
  output: 'version.h',
)

subdir('src')
