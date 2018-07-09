project('dxvk', ['c', 'cpp'])

cpu_family = target_machine.cpu_family()

dxvk_compiler = meson.get_compiler('cpp')
if dxvk_compiler.get_id() == 'msvc'
  add_project_arguments('-DNOMINMAX', language : 'cpp')
  dxvk_cpp_std='c++latest'
else
  dxvk_cpp_std='c++1z'
endif

# DXVK version
is_head = find_program('head', required : false).found()
is_git  = find_program('git', required : false).found()
if is_head
  dxvk_ver = run_command('head', '-n1', 'RELEASE').stdout().strip()
  if dxvk_ver != '' and is_git
    dxvk_git_ver = run_command('git', 'rev-parse', '--short', 'HEAD').stdout().strip()
    if dxvk_git_ver != ''
      dxvk_ver += ' (' + dxvk_git_ver + ')'
    endif
    add_project_arguments('-DDXVK_VERSION="' + dxvk_ver + '"', language : 'cpp')
  endif
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

lib_vulkan  = dxvk_compiler.find_library('vulkan-1', dirs : dxvk_library_path)
lib_d3d11   = dxvk_compiler.find_library('d3d11')
lib_dxgi    = dxvk_compiler.find_library('dxgi')

if dxvk_compiler.get_id() != 'msvc'
  lib_d3dcompiler_47 = dxvk_compiler.find_library('d3dcompiler_47')
endif

glsl_compiler = find_program('glslangValidator')
glsl_generator = generator(glsl_compiler,
  output    : [ '@BASENAME@.h' ],
  arguments : [ '-V', '--vn', '@BASENAME@', '@INPUT@', '-o', '@OUTPUT@' ])

subdir('src')

enable_tests = get_option('enable_tests')

if enable_tests
  subdir('tests')
endif

if dxvk_compiler.get_id() != 'msvc'
  subdir('wine_utils')
endif
