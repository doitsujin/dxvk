test_dxbc_deps = [ dxbc_dep, dxvk_dep ]

executable('dxbc-compiler'+exe_ext, files('test_dxbc_compiler.cpp'), dependencies : test_dxbc_deps, install : true, gui_app : false)
executable('hlsl-compiler'+exe_ext, files('test_hlsl_compiler.cpp'), dependencies : [ test_dxbc_deps, lib_d3dcompiler_47 ], install : true, gui_app : false)

if platform == 'windows'
executable('dxbc-disasm'+exe_ext,   files('test_dxbc_disasm.cpp'),   dependencies : [ test_dxbc_deps, lib_d3dcompiler_47 ], install : true, gui_app : true)
endif
