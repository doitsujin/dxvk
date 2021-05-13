test_d3d11_deps = [ util_dep, lib_dxgi, lib_d3d11, lib_d3dcompiler_47 ]

executable('d3d11-compute'+exe_ext,   files('test_d3d11_compute.cpp'),   dependencies : test_d3d11_deps, install : true, gui_app : true, override_options: ['cpp_std='+dxvk_cpp_std])
executable('d3d11-formats'+exe_ext,   files('test_d3d11_formats.cpp'),   dependencies : test_d3d11_deps, install : true, gui_app : true, override_options: ['cpp_std='+dxvk_cpp_std])
executable('d3d11-map-read'+exe_ext,  files('test_d3d11_map_read.cpp'),  dependencies : test_d3d11_deps, install : true, gui_app : true, override_options: ['cpp_std='+dxvk_cpp_std])
executable('d3d11-streamout'+exe_ext, files('test_d3d11_streamout.cpp'), dependencies : test_d3d11_deps, install : true, gui_app : true, override_options: ['cpp_std='+dxvk_cpp_std])
executable('d3d11-triangle'+exe_ext,  files('test_d3d11_triangle.cpp'),  dependencies : test_d3d11_deps, install : true, gui_app : true, override_options: ['cpp_std='+dxvk_cpp_std])
executable('d3d11-video'+exe_ext,     files('test_d3d11_video.cpp'),     dependencies : test_d3d11_deps, install : true, gui_app : true, override_options: ['cpp_std='+dxvk_cpp_std])

install_data('video_image.raw', install_dir : get_option('bindir'))