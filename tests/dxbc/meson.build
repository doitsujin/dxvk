test_dxbc_deps = [ dxbc_dep, dxvk_dep ]

executable('dxbc-compiler', files('test_dxbc_compiler.cpp'), dependencies : test_dxbc_deps, install : true)
executable('dxbc-disasm',   files('test_dxbc_disasm.cpp'),   dependencies : [ test_dxbc_deps, lib_d3dcompiler_47 ], install : true)