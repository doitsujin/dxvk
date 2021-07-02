project('dxvk', ['c', 'cpp'], version : 'v1.9', meson_version : '>= 0.46')

cpu_family = target_machine.cpu_family()

add_project_arguments('-DNOMINMAX', language : 'cpp')

dxvk_compiler = meson.get_compiler('cpp')
dxvk_is_msvc = dxvk_compiler.get_id() == 'msvc'

# c++17 was added in 15.3, older version needs c++latest
if dxvk_is_msvc and dxvk_compiler.version().version_compare('<15.3')
  dxvk_cpp_std='c++latest'
else
  dxvk_cpp_std='c++17'
endif

if dxvk_is_msvc
  add_project_arguments('/std:' + dxvk_cpp_std, language : 'cpp')
endif

if dxvk_compiler.get_id() == 'clang'
  if dxvk_compiler.has_argument('-Wno-unused-private-field')
    add_project_arguments('-Wno-unused-private-field', language: 'cpp')
  endif
  if dxvk_compiler.has_argument('-Wno-microsoft-exception-spec')
    add_project_arguments('-Wno-microsoft-exception-spec', language: 'cpp')
  endif
endif

if not dxvk_is_msvc
  add_project_arguments('-D_WIN32_WINNT=0xa00', language : 'cpp')

  if get_option('build_id') and dxvk_compiler.has_link_argument('-Wl,--build-id')
    add_global_link_arguments('-Wl,--build-id', language: 'cpp')
  endif

  # We need to set the section alignment for debug symbols to
  # work properly as well as avoiding a memcpy from the Wine loader.
  if dxvk_compiler.has_link_argument('-Wl,--file-alignment=4096')
    add_global_link_arguments('-Wl,--file-alignment=4096', language: 'cpp')
  endif
endif

dxvk_include_path = include_directories('./include')

if (cpu_family == 'x86_64')
  dxvk_library_path = meson.source_root() + '/lib'
else
  dxvk_library_path = meson.source_root() + '/lib32'
endif

dxvk_extradep = [ ]

if dxvk_is_msvc
  wrc = find_program('rc')
else
  add_global_link_arguments('-static', '-static-libgcc', language: 'c')
  add_global_link_arguments('-static', '-static-libgcc', '-static-libstdc++', language: 'cpp')

  wrc = find_program('windres')
endif

if cpu_family == 'x86_64'
  if dxvk_compiler.has_argument('-msse3')
    add_project_arguments('-msse3', language: ['c', 'cpp'])
  endif
elif cpu_family == 'x86'
  if dxvk_compiler.has_link_argument('-Wl,--add-stdcall-alias')
    add_global_link_arguments('-Wl,--add-stdcall-alias', language: 'cpp')
  endif
  if dxvk_compiler.has_link_argument('-Wl,--enable-stdcall-fixup')
    add_global_link_arguments('-Wl,--enable-stdcall-fixup', language: 'cpp')
  endif
  if dxvk_compiler.has_argument('-msse') and dxvk_compiler.has_argument('-msse2') and dxvk_compiler.has_argument('-msse3')
    add_project_arguments('-msse', '-msse2', '-msse3', language: ['c', 'cpp'])
  endif
  if dxvk_compiler.has_argument('-mfpmath=sse')
    add_project_arguments('-mfpmath=sse', language: ['c', 'cpp'])
  endif
endif

lib_vulkan  = dxvk_compiler.find_library('vulkan-1', dirs : dxvk_library_path)
lib_d3d9    = dxvk_compiler.find_library('d3d9')
lib_d3d11   = dxvk_compiler.find_library('d3d11')
lib_dxgi    = dxvk_compiler.find_library('dxgi')
lib_d3dcompiler_43 = dxvk_compiler.find_library('d3dcompiler_43', dirs : dxvk_library_path)

if dxvk_is_msvc
  lib_d3dcompiler_47 = dxvk_compiler.find_library('d3dcompiler')
else
  lib_d3dcompiler_47 = dxvk_compiler.find_library('d3dcompiler_47')
endif

exe_ext = ''
dll_ext = ''

if dxvk_is_msvc
  res_ext = '.res'
else
  res_ext = '.o'
endif

def_spec_ext = '.def'

glsl_compiler = find_program('glslangValidator')
glsl_args = [ '-V', '--vn', '@BASENAME@', '@INPUT@', '-o', '@OUTPUT@' ]
if run_command(glsl_compiler, [ '--quiet', '--version' ]).returncode() == 0
    glsl_args += [ '--quiet' ]
endif
glsl_generator = generator(glsl_compiler,
  output    : [ '@BASENAME@.h' ],
  arguments : glsl_args)

if dxvk_is_msvc
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
