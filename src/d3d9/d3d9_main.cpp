#include "../dxvk/dxvk_instance.h"

#include "d3d9_interface.h"
#include "d3d9_shader_validator.h"

#include "d3d9_annotation.h"

class D3DFE_PROCESSVERTICES;
using PSGPERRORID = UINT;

namespace dxvk {
  Logger Logger::s_instance("d3d9.log");
  D3D9GlobalAnnotationList D3D9GlobalAnnotationList::s_instance;
  
   // 初始化日志记录的函数
  void InitializeLogging() {
    dxvk::Logger::info("如果你的程序闪退且日志文件没后续内容, 那可能是没有Vulkan 1.3 驱动 请更新显卡驱动到最新");
    dxvk::Logger::info("常见解决方案：如果你的CPU是AMD的 有可能是核显驱动有问题 请重新打一下AMD显卡驱动即可解决!");
  }

  // 静态初始化器结构体
  struct StaticInitializer {
    StaticInitializer() {
      InitializeLogging();
    }
  };
  // 静态实例，确保程序启动时调用InitializeLogging
  static StaticInitializer s_initializer;

  HRESULT CreateD3D9(
          bool           Extended,
          IDirect3D9Ex** ppDirect3D9Ex) {
    if (!ppDirect3D9Ex)
      return D3DERR_INVALIDCALL;

    *ppDirect3D9Ex = ref(new D3D9InterfaceEx( Extended ));
    return D3D_OK;
  }
}


extern "C" {

  DLLEXPORT IDirect3D9* __stdcall Direct3DCreate9(UINT nSDKVersion) {
    IDirect3D9Ex* pDirect3D = nullptr;
    dxvk::CreateD3D9(false, &pDirect3D);

    return pDirect3D;
  }

  DLLEXPORT HRESULT __stdcall Direct3DCreate9Ex(UINT nSDKVersion, IDirect3D9Ex** ppDirect3D9Ex) {
    return dxvk::CreateD3D9(true, ppDirect3D9Ex);
  }

  DLLEXPORT int __stdcall D3DPERF_BeginEvent(D3DCOLOR col, LPCWSTR wszName) {
    return dxvk::D3D9GlobalAnnotationList::Instance().BeginEvent(col, wszName);
  }

  DLLEXPORT int __stdcall D3DPERF_EndEvent(void) {
    return dxvk::D3D9GlobalAnnotationList::Instance().EndEvent();
  }

  DLLEXPORT void __stdcall D3DPERF_SetMarker(D3DCOLOR col, LPCWSTR wszName) {
    dxvk::D3D9GlobalAnnotationList::Instance().SetMarker(col, wszName);
  }

  DLLEXPORT void __stdcall D3DPERF_SetRegion(D3DCOLOR col, LPCWSTR wszName) {
    dxvk::D3D9GlobalAnnotationList::Instance().SetRegion(col, wszName);
  }

  DLLEXPORT BOOL __stdcall D3DPERF_QueryRepeatFrame(void) {
    return dxvk::D3D9GlobalAnnotationList::Instance().QueryRepeatFrame();
  }

  DLLEXPORT void __stdcall D3DPERF_SetOptions(DWORD dwOptions) {
    dxvk::D3D9GlobalAnnotationList::Instance().SetOptions(dwOptions);
  }

  DLLEXPORT DWORD __stdcall D3DPERF_GetStatus(void) {
    return dxvk::D3D9GlobalAnnotationList::Instance().GetStatus();
  }


  DLLEXPORT void __stdcall DebugSetMute(void) {
  }

  DLLEXPORT int __stdcall DebugSetLevel(void) {
    return 0;
  }

  // Processor Specific Geometry Pipeline
  // for P3 SIMD/AMD 3DNow.

  DLLEXPORT void __stdcall PSGPError(D3DFE_PROCESSVERTICES* a, PSGPERRORID b, UINT c) {
  }

  DLLEXPORT void __stdcall PSGPSampleTexture(D3DFE_PROCESSVERTICES* a, UINT b, float(*const c)[4], UINT d, float(*const e)[4]) {
  }

  DLLEXPORT dxvk::D3D9ShaderValidator* __stdcall Direct3DShaderValidatorCreate9(void) {
    return ref(new dxvk::D3D9ShaderValidator());
  }

  DLLEXPORT int __stdcall Direct3D9EnableMaximizedWindowedModeShim(UINT a) {
    return 0;
  }

  DLLEXPORT void __stdcall DXVK_RegisterAnnotation(IDXVKUserDefinedAnnotation* annotation) {
    dxvk::D3D9GlobalAnnotationList::Instance().RegisterAnnotator(annotation);
  }

  DLLEXPORT void __stdcall DXVK_UnRegisterAnnotation(IDXVKUserDefinedAnnotation* annotation) {
    dxvk::D3D9GlobalAnnotationList::Instance().UnregisterAnnotator(annotation);
  }

  DLLEXPORT void __stdcall Direct3D9ForceHybridEnumeration(UINT uHybrid) {
  }

  DLLEXPORT IDirect3D9* __stdcall Direct3DCreate9On12(UINT sdk_version, D3D9ON12_ARGS* override_list, UINT override_entry_count) {
    dxvk::Logger::warn("Direct3DCreate9On12: 9On12 functionality is unimplemented.");
    return Direct3DCreate9(sdk_version);
  }

  DLLEXPORT HRESULT __stdcall Direct3DCreate9On12Ex(UINT sdk_version, D3D9ON12_ARGS* override_list, UINT override_entry_count, IDirect3D9Ex** output) {
    dxvk::Logger::warn("Direct3DCreate9On12Ex: 9On12 functionality is unimplemented.");
    return Direct3DCreate9Ex(sdk_version, output);
  }

}