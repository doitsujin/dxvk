@ stdcall D3D10CreateDevice1(ptr long ptr long long long ptr)
@ stdcall D3D10CreateDeviceAndSwapChain1(ptr long ptr long long long ptr ptr ptr)

@ stdcall D3D10GetVertexShaderProfile(ptr)
@ stdcall D3D10GetGeometryShaderProfile(ptr)
@ stdcall D3D10GetPixelShaderProfile(ptr)

@ stdcall D3D10CreateBlob(long ptr)
@ stdcall D3D10GetInputSignatureBlob(ptr long ptr)
@ stdcall D3D10GetOutputSignatureBlob(ptr long ptr)

@ stdcall D3D10ReflectShader(ptr long ptr)
@ stdcall D3D10CompileShader(ptr long str ptr ptr str str long ptr ptr)

@ stdcall D3D10CreateEffectFromMemory(ptr long long ptr ptr ptr)
@ stdcall D3D10CreateEffectPoolFromMemory(ptr long long ptr ptr)
@ stdcall D3D10CompileEffectFromMemory(ptr long ptr ptr ptr long long ptr ptr)

@ stub D3D10DisassembleEffect
@ stdcall D3D10DisassembleShader(ptr long long ptr ptr)
@ stub D3D10PreprocessShader

@ stdcall D3D10CreateStateBlock(ptr ptr ptr)
@ stdcall D3D10StateBlockMaskDifference(ptr ptr ptr)
@ stdcall D3D10StateBlockMaskDisableAll(ptr)
@ stdcall D3D10StateBlockMaskDisableCapture(ptr long long long)
@ stdcall D3D10StateBlockMaskEnableAll(ptr)
@ stdcall D3D10StateBlockMaskEnableCapture(ptr long long long)
@ stdcall D3D10StateBlockMaskGetSetting(ptr long long)
@ stdcall D3D10StateBlockMaskIntersect(ptr ptr ptr)
@ stdcall D3D10StateBlockMaskUnion(ptr ptr ptr)
