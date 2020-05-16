project('dxvk', ['c', 'cpp'], version : 'v1.7', meson_version : '>= 0.46')

cpu_family = target_machine.cpu_family()

add_project_arguments('-DNOMINMAX', language : 'cpp')

dxvk_compiler = meson.get_compiler('cpp')
dxvk_msvc=dxvk_compiler.get_id() == 'msvc'
# c++17 was added in 15.3, older version needs c++latest
if dxvk_compiler.get_id() == 'msvc' and dxvk_compiler.version().version_compare('<15.3')
  dxvk_cpp_std='c++latest'
else
  dxvk_cpp_std='c++17'
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

dxvk_extradep = [ ]

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
