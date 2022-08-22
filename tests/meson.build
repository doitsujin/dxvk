if platform != 'windows'
    lib_vkd3d_shader = dependency('libvkd3d-shader')

    lib_mini_d3dcompiler = static_library('mini_d3dcompiler', [ 'test_d3dcompiler.cpp' ],
        dependencies        : [ lib_vkd3d_shader ],
        include_directories : [ dxvk_include_path ],
    )

    lib_d3dcompiler_47 = declare_dependency(
        link_with           : [ lib_mini_d3dcompiler ],
        include_directories : [ dxvk_include_path ],
    )
endif

subdir('d3d9')
subdir('d3d11')
subdir('dxbc')
subdir('dxgi')
