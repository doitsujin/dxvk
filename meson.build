project('dxvk', ['c', 'cpp'], version : 'v1.4.6', meson_version : '>= 0.46')

cpu_family = target_machine.cpu_family()

add_project_arguments('-DNOMINMAX', language : 'cpp')

dxvk_compiler = meson.get_compiler('cpp')
if dxvk_compiler.get_id() == 'msvc'
  dxvk_cpp_std='c++latest'
  dxvk_msvc=true
else
  dxvk_cpp_std='c++17'
  dxvk_msvc=false
endif

if dxvk_compiler.get_id() == 'msvc'
  add_project_arguments('/std:' + dxvk_cpp_std, language : 'cpp')
endif

dxvk_include_path = include_directories('./include')

if (cpu_family == 'x86_64')
  dxvk_library_path = meson.source_root() + '/lib'
else
  dxvk_library_path = meson.source_root() + '/lib32'
endif

code = '''#ifndef __WINE__
#error 1
#endif'''

dxvk_winelib = dxvk_compiler.compiles(code, name: 'winelib check')
dxvk_extradep = [ ]

if dxvk_winelib
  if dxvk_compiler.has_argument('--no-gnu-unique')
    add_project_arguments('--no-gnu-unique', language : ['cpp'])
  endif
  wrc = find_program('wrc')
  lib_vulkan  = declare_dependency(link_args: [ '-lwinevulkan' ])
  lib_d3d9    = declare_dependency(link_args: [ '-ld3d9' ])
  lib_d3d11   = declare_dependency(link_args: [ '-ld3d11' ])
  lib_dxgi    = declare_dependency(link_args: [ '-ldxgi' ])
  lib_d3dcompiler_43 = declare_dependency(link_args: [ '-L'+dxvk_library_path, '-ld3dcompiler_43' ])
  lib_d3dcompiler_47 = declare_dependency(link_args: [ '-ld3dcompiler' ])
  dxvk_extradep += [ declare_dependency(link_args: [ '-ldl' ]) ]
  exe_ext = '.exe.so'
  dll_ext = '.dll'
  res_ext = '.res'
  def_spec_ext = '.spec'
else
  if dxvk_compiler.get_id() == 'msvc'
    wrc = find_program('rc')
  else
    wrc = cpu_family == 'x86_64' ? find_program('x86_64-w64-mingw32-windres') : find_program('i686-w64-mingw32-windres')
  endif
  if cpu_family == 'x86'
    if dxvk_compiler.has_link_argument('-Wl,--add-stdcall-alias')
      add_global_link_arguments('-Wl,--add-stdcall-alias', language: 'cpp')
    endif
    if dxvk_compiler.has_link_argument('-Wl,--enable-stdcall-fixup')
      add_global_link_arguments('-Wl,--enable-stdcall-fixup', language: 'cpp')
    endif
  endif

  lib_vulkan  = dxvk_compiler.find_library('vulkan-1', dirs : dxvk_library_path)
  lib_d3d9    = dxvk_compiler.find_library('d3d9')
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

  if dxvk_compiler.get_id() == 'msvc'
    res_ext = '.res'
  else
    res_ext = '.o'
  endif

  def_spec_ext = '.def'
endif

glsl_compiler = find_program('glslangValidator')
glsl_generator = generator(glsl_compiler,
  output    : [ '@BASENAME@.h' ],
  arguments : [ '-V', '--vn', '@BASENAME@', '@INPUT@', '-o', '@OUTPUT@' ])

if dxvk_compiler.get_id() == 'msvc'
  wrc_generator = generator(wrc,
  output    : [ '@BASENAME@' + res_ext ],
  arguments : [ '/fo', '@OUTPUT@', '@INPUT@' ])
else
  wrc_generator = generator(wrc,
  output    : [ '@BASENAME@' + res_ext ],
  arguments : [ '-i', '@INPUT@', '-o', '@OUTPUT@' ])
endif

dxvk_version = vcs_tag(
  command: ['git', 'describe', '--dirty=+'],
  input:  'version.h.in',
  output: 'version.h')

subdir('src')

enable_tests = get_option('enable_tests')

if enable_tests
  subdir('tests')
endif
