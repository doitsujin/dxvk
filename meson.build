project('dxvk', ['c', 'cpp'], version : 'v1.10.3', meson_version : '>= 0.49', default_options : [ 'cpp_std=c++17' ])

cpu_family = target_machine.cpu_family()

cpp = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
dxvk_is_msvc = cpp.get_id() == 'msvc'

compiler_args = [
  '-DNOMINMAX',
  '-D_WIN32_WINNT=0xa00',
  '-msse',
  '-msse2',
  '-msse3',
  '-mfpmath=sse',
  '-Wimplicit-fallthrough',
  # clang
  '-Wno-unused-private-field',
  '-Wno-microsoft-exception-spec',
]

link_args = [
  '-static',
  '-static-libgcc',
  '-static-libstdc++',
  # We need to set the section alignment for debug symbols to
  # work properly as well as avoiding a memcpy from the Wine loader.
  '-Wl,--file-alignment=4096',
]

# Wine's built-in back traces only work with dwarf2 symbols
if get_option('debug') and target_machine.system() == 'windows'
  compiler_args += [
    '-gstrict-dwarf',
    '-gdwarf-2',
  ]
endif

if get_option('build_id')
  link_args += [
    '-Wl,--build-id',
  ]
endif

if cpu_family == 'x86'
  link_args += [
    '-Wl,--enable-stdcall-fixup',
    '-Wl,--add-stdcall-alias',
  ]
endif

add_project_arguments(cpp.get_supported_arguments(compiler_args), language: 'cpp')
add_project_arguments(cc.get_supported_arguments(compiler_args), language: 'c')
add_project_link_arguments(cpp.get_supported_link_arguments(link_args), language: 'cpp')
add_project_link_arguments(cc.get_supported_link_arguments(link_args), language: 'c')

dxvk_include_path = include_directories('./include')

if (cpu_family == 'x86_64')
  dxvk_library_path = meson.source_root() + '/lib'
else
  dxvk_library_path = meson.source_root() + '/lib32'
endif

dxvk_extradep = [ ]

lib_vulkan  = cpp.find_library('vulkan-1', dirs : dxvk_library_path)
lib_d3d9    = cpp.find_library('d3d9')
lib_d3d11   = cpp.find_library('d3d11')
lib_dxgi    = cpp.find_library('dxgi')
lib_d3dcompiler_43 = cpp.find_library('d3dcompiler_43', dirs : dxvk_library_path)

if dxvk_is_msvc
  lib_d3dcompiler_47 = cpp.find_library('d3dcompiler')
else
  lib_d3dcompiler_47 = cpp.find_library('d3dcompiler_47')
endif

exe_ext = ''
dll_ext = ''

def_spec_ext = '.def'

glsl_compiler = find_program('glslangValidator')
glsl_args = [ '-V', '--vn', '@BASENAME@', '@INPUT@', '-o', '@OUTPUT@' ]
if run_command(glsl_compiler, [ '--quiet', '--version' ], check : false).returncode() == 0
    glsl_args += [ '--quiet' ]
endif
glsl_generator = generator(glsl_compiler,
  output    : [ '@BASENAME@.h' ],
  arguments : glsl_args,
)

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
