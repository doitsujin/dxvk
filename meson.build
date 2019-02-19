project('dxvk', ['c', 'cpp'], version : 'v0.96', meson_version : '>= 0.46')

cpu_family = target_machine.cpu_family()

add_project_arguments('-DNOMINMAX', language : 'cpp')

dxvk_compiler = meson.get_compiler('cpp')
if dxvk_compiler.get_id() == 'msvc'
  dxvk_cpp_std='c++latest'
  dxvk_msvc=true
else
  dxvk_cpp_std='c++1z'
  dxvk_msvc=false
endif

if dxvk_compiler.get_id() == 'msvc'
  add_project_arguments('/std:' + dxvk_cpp_std, language : 'cpp')
endif

if (cpu_family == 'x86_64')
  dxvk_library_path = meson.source_root() + '/lib'
else
  dxvk_library_path = meson.source_root() + '/lib32'
endif

winelib_check_code = '''#ifndef __WINE__
#error 1
#endif'''

mingw_check_code = '''#ifndef __MINGW32__
#error 1
#endif'''

dxvk_winelib = dxvk_compiler.compiles(winelib_check_code, name: 'winelib check')
dxvk_mingw   = dxvk_compiler.compiles(mingw_check_code, name: 'mingw check')

dxvk_native = not dxvk_mingw and not dxvk_winelib and dxvk_compiler.get_id() != 'msvc'

dxvk_include_path = include_directories(['./include', dxvk_native ? './include/native' : ''])

dxvk_extradep = [ ]

if dxvk_winelib
  lib_vulkan  = declare_dependency(link_args: [ '-lwinevulkan' ])
  lib_d3d11   = declare_dependency(link_args: [ '-ld3d11' ])
  lib_dxgi    = declare_dependency(link_args: [ '-ldxgi' ])
  lib_d3dcompiler_43 = declare_dependency(link_args: [ '-L'+dxvk_library_path, '-ld3dcompiler_43' ])
  lib_d3dcompiler_47 = declare_dependency(link_args: [ '-ld3dcompiler' ])
  dxvk_extradep += [ declare_dependency(link_args: [ '-ldl' ]) ]
  exe_ext = '.exe.so'
  dll_ext = '.dll'
  def_spec_ext = '.spec'
elif dxvk_native
  lib_vulkan = dxvk_compiler.find_library('vulkan', dirs : dxvk_library_path)
  lib_dxgi   = dependency('', required : false)
  lib_d3dcompiler_43 = dependency('', required : false)
  lib_d3dcompiler_47 = dependency('', required : false)
  dxvk_extradep += [ declare_dependency(link_args: [ '-ldl' ]) ]
  dxvk_extradep += [ declare_dependency(link_args: [ '-pthread' ]) ]

  idl_compiler = find_program('widl')

  com_header_generator = generator(idl_compiler,
    output    : [ '@BASENAME@.h' ],
    arguments : [ '-h', '-o', '@OUTPUT@', '@INPUT@' ])

  exe_ext = ''
  dll_ext = ''
  def_spec_ext = '.def'
  
  add_project_arguments(['-DDXVK_NATIVE=1'], language : 'cpp')
else
  lib_vulkan  = dxvk_compiler.find_library('vulkan-1', dirs : dxvk_library_path)
  lib_d3d11   = dxvk_compiler.find_library('d3d11')
  lib_dxgi    = dxvk_compiler.find_library('dxgi')
  lib_d3dcompiler_43 = dxvk_compiler.find_library('d3dcompiler_43', dirs : dxvk_library_path)
  
  if dxvk_compiler.get_id() == 'msvc'
    lib_d3dcompiler_47 = dxvk_compiler.find_library('d3dcompiler')
  else
    lib_d3dcompiler_47 = dxvk_compiler.find_library('d3dcompiler_47')
  endif
  exe_ext = ''
  dll_ext = ''
  def_spec_ext = '.def'
endif

glsl_compiler = find_program('glslangValidator')
glsl_generator = generator(glsl_compiler,
  output    : [ '@BASENAME@.h' ],
  arguments : [ '-V', '--vn', '@BASENAME@', '@INPUT@', '-o', '@OUTPUT@' ])

dxvk_version = vcs_tag(
  command: ['git', 'describe', '--dirty=+'],
  input:  'version.h.in',
  output: 'version.h')

if dxvk_native
  subdir('include')
endif

subdir('src')

enable_tests = get_option('enable_tests')

if enable_tests
  subdir('tests')
endif

if dxvk_compiler.get_id() != 'msvc' and not dxvk_native
  subdir('utils')
endif

if dxvk_native
  pkg = import('pkgconfig')
  pkg.generate(
    d3d11_dll,
    description : 'D3D10-11 Layer On Vulkan',
    subdirs : 'dxvk')
endif
