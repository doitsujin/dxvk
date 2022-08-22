#include <cstring>
#include <fstream>
#include <vector>

#include <windows.h>
#include <d3dcompiler.h>

#include <vkd3d/vkd3d_shader.h>

#include "test_utils.h"

using namespace dxvk;

class D3DBlob final : public ComObject<ID3DBlob> {
public:
  D3DBlob(const uint8_t* data, size_t size)
    : m_data(data, data + size) {
  }

  D3DBlob(const void* data, size_t size)
    : D3DBlob(reinterpret_cast<const uint8_t*>(data), size) {
  }

  HRESULT QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3DBlob)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    std::cerr << "D3DBlob::QueryInterface: Unknown interface query";
    std::cerr << str::format(riid);
    return E_NOINTERFACE;
  }

  void*  STDMETHODCALLTYPE GetBufferPointer() { return m_data.data(); }
  SIZE_T STDMETHODCALLTYPE GetBufferSize()    { return m_data.size(); }
private:
  std::vector<uint8_t> m_data;
};

static const size_t GetMacroCount(const D3D_SHADER_MACRO *pDefines) {
  uint32_t count = 0;
  while (pDefines && pDefines->Name) {
    pDefines++;
    count++;
  }
  return count;
}

HRESULT WINAPI D3DCompile2(
        LPCVOID                pSrcData,
        SIZE_T                 SrcDataSize,
        LPCSTR                 pSourceName,
        const D3D_SHADER_MACRO *pDefines,
        ID3DInclude            *pInclude,
        LPCSTR                 pEntrypoint,
        LPCSTR                 pTarget,
        UINT                   Flags1,
        UINT                   Flags2,
        UINT                   SecondaryDataFlags,
        LPCVOID                pSecondaryData,
        SIZE_T                 SecondaryDataSize,
        ID3DBlob               **ppCode,
        ID3DBlob               **ppErrorMsgs) {
  InitReturnPtr(ppCode);
  InitReturnPtr(ppErrorMsgs);

  vkd3d_shader_compile_option option = { VKD3D_SHADER_COMPILE_OPTION_API_VERSION, VKD3D_SHADER_API_VERSION_1_4 };

  vkd3d_shader_preprocess_info preprocessInfo = { VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO };
  preprocessInfo.macros      = reinterpret_cast<const vkd3d_shader_macro*>(pDefines);
  preprocessInfo.macro_count = GetMacroCount(pDefines);
  // Does not handle pInclude right now...
  // See pfn_open_include & friends in vkd3d_shader_preprocess_info.

  vkd3d_shader_hlsl_source_info hlslInfo = { VKD3D_SHADER_STRUCTURE_TYPE_HLSL_SOURCE_INFO };
  hlslInfo.next              = &preprocessInfo;
  hlslInfo.entry_point       = pEntrypoint;
  hlslInfo.secondary_code    = vkd3d_shader_code{ pSecondaryData, SecondaryDataSize };
  hlslInfo.profile           = pTarget;

  vkd3d_shader_compile_info compileInfo = { VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO };
  compileInfo.next           = &hlslInfo;
  compileInfo.source.code    = pSrcData;
  compileInfo.source.size    = SrcDataSize;
  compileInfo.source_type    = VKD3D_SHADER_SOURCE_HLSL;
  // Check for eg. ps_3 or below below.
  compileInfo.target_type    = pTarget[3] <= '3' ?  VKD3D_SHADER_TARGET_D3D_BYTECODE : VKD3D_SHADER_TARGET_DXBC_TPF;
  compileInfo.options        = &option;
  compileInfo.option_count   = 1;
  compileInfo.log_level      = VKD3D_SHADER_LOG_INFO;
  compileInfo.source_name    = pSourceName;

  vkd3d_shader_code outCode = { };
  char* messages = nullptr;
  int ret = vkd3d_shader_compile(&compileInfo, &outCode, &messages);

  if (!ret && ppCode)
    *ppCode = ref(new D3DBlob(outCode.code, outCode.size));

  if (messages && ppErrorMsgs)
    *ppErrorMsgs = ref(new D3DBlob(messages, strlen(messages)));

  vkd3d_shader_free_messages(messages);

  return ret == 0 ? S_OK : E_FAIL;
};

HRESULT WINAPI D3DCompile(
        LPCVOID                pSrcData,
        SIZE_T                 SrcDataSize,
        LPCSTR                 pSourceName,
        const D3D_SHADER_MACRO *pDefines,
        ID3DInclude            *pInclude,
        LPCSTR                 pEntrypoint,
        LPCSTR                 pTarget,
        UINT                   Flags1,
        UINT                   Flags2,
        ID3DBlob               **ppCode,
        ID3DBlob               **ppErrorMsgs) {
  return D3DCompile2(
    pSrcData, SrcDataSize, pSourceName, pDefines, pInclude,
    pEntrypoint, pTarget, Flags1, Flags2,
    0, nullptr, 0,
    ppCode, ppErrorMsgs);
}
