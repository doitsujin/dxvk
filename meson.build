project('dxvk', ['c', 'cpp'], version : 'v1.10.1', meson_version : '>= 0.49', default_options : [ 'cpp_std=c++17' ])

cpu_family = target_machine.cpu_family()
platform   = target_machine.system()

cpp = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
dxvk_is_msvc = cpp.get_id() == 'msvc'

compiler_args = [
  '-msse',
  '-msse2',
  '-msse3',
  '-mfpmath=sse',
  '-Wimplicit-fallthrough',
  # clang
  '-Wno-unused-private-field',
  '-Wno-microsoft-exception-spec',
]

link_args = []

if get_option('build_id')
  link_args += [
    '-Wl,--build-id',
  ]
endif

dxvk_include_dirs = [
  './include',
  './include/vulkan/include',
  './include/spirv/include'
]

if platform == 'windows'
  compiler_args += [
    '-DNOMINMAX',
    '-D_WIN32_WINNT=0xa00',
  ]

  link_args += [
    '-static',
    '-static-libgcc',
    '-static-libstdc++',
    # We need to set the section alignment for debug symbols to
    # work properly as well as avoiding a memcpy from the Wine loader.
    '-Wl,--file-alignment=4096',
  ]

  # Wine's built-in back traces only work with dwarf2 symbols
  if get_option('debug')
    compiler_args += [
      '-gstrict-dwarf',
      '-gdwarf-2',
    ]
  endif

  # Enable stdcall fixup on 32-bit
  if cpu_family == 'x86'
    link_args += [
      '-Wl,--enable-stdcall-fixup',
      '-Wl,--add-stdcall-alias',
  ]
  endif

  if (cpu_family == 'x86_64')
    dxvk_library_path = meson.current_source_dir() + '/lib'
  else
    dxvk_library_path = meson.current_source_dir() + '/lib32'
  endif

  lib_d3d9    = cpp.find_library('d3d9')
  lib_d3d11   = cpp.find_library('d3d11')
  lib_dxgi    = cpp.find_library('dxgi')
  lib_d3dcompiler_43 = cpp.find_library('d3dcompiler_43', dirs : dxvk_library_path)

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
  compiler_args += ['-DDXVK_WSI_WIN32']
else
  lib_sdl2   = cpp.find_library('SDL2')

  wrc           = find_program('touch')
  wrc_generator = generator(wrc, output : [ '@BASENAME@_ignored.h' ], arguments : [ '@OUTPUT@' ] )

  dxvk_include_dirs += [
    './include/native',
    './include/native/windows',
    './include/native/directx'
  ]

  dxvk_wsi = 'sdl2'
  compiler_args += ['-DDXVK_WSI_SDL2']
endif

dxvk_include_path = include_directories(dxvk_include_dirs)

add_project_arguments(cpp.get_supported_arguments(compiler_args), language: 'cpp')
add_project_arguments(cc.get_supported_arguments(compiler_args), language: 'c')
add_project_link_arguments(cpp.get_supported_link_arguments(link_args), language: 'cpp')
add_project_link_arguments(cc.get_supported_link_arguments(link_args), language: 'c')

exe_ext = ''
dll_ext = ''
def_spec_ext = '.def'

glsl_compiler = find_program('glslangValidator')
glsl_args = [ '--target-env', 'vulkan1.2', '--vn', '@BASENAME@', '@INPUT@', '-o', '@OUTPUT@' ]
if run_command(glsl_compiler, [ '--quiet', '--version' ], check : false).returncode() == 0
    glsl_args += [ '--quiet' ]
endif
glsl_generator = generator(glsl_compiler,
  output    : [ '@BASENAME@.h' ],
  arguments : glsl_args,
)

dxvk_version = vcs_tag(
  command: ['git', 'describe', '--dirty=+'],
  input:  'version.h.in',
  output: 'version.h',
)

subdir('src')

enable_tests = get_option('enable_tests')

if enable_tests
  subdir('tests')
endif
