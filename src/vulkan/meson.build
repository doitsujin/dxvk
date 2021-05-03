vkcommon_src = files([
  'vulkan_loader.cpp',
  'vulkan_names.cpp',
  'vulkan_presenter.cpp',
])

thread_dep = dependency('threads')

vkcommon_opts = common_static_opts + { 'dependencies': [ thread_dep, lib_vulkan ] }

vkcommon_lib = static_library('vkcommon', vkcommon_src, kwargs: vkcommon_opts)

vkcommon_dep = declare_dependency(
  link_with           : [ vkcommon_lib ],
  include_directories : [ dxvk_include_path ])
