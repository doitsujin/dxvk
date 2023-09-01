#pragma once

#include "d3d9_device.h"
#include "d3d9_include.h"

#include "../dxvk/dxvk_annotation.h"

#include "../util/thread.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace dxvk {

  class D3D9GlobalAnnotationList {

  public:

    D3D9GlobalAnnotationList();
    ~D3D9GlobalAnnotationList();

    void RegisterAnnotator(IDXVKUserDefinedAnnotation* annotation);
    void UnregisterAnnotator(IDXVKUserDefinedAnnotation* annotation);

    INT BeginEvent(D3DCOLOR color, LPCWSTR name);
    INT EndEvent();

    void SetMarker(D3DCOLOR color, LPCWSTR name);

    void SetRegion(D3DCOLOR color, LPCWSTR name);

    BOOL QueryRepeatFrame() const;

    void SetOptions(DWORD options);

    DWORD GetStatus() const;

    static D3D9GlobalAnnotationList& Instance() {
      return s_instance;
    }

  private:

    static D3D9GlobalAnnotationList s_instance;

    std::atomic<bool> m_shouldAnnotate;

    dxvk::mutex m_mutex;
    std::vector<IDXVKUserDefinedAnnotation*> m_annotations;

    // Provide our own event depth as we
    // may have multiple annotators which could get out of sync.
    int32_t m_eventDepth;

  };

  class D3D9UserDefinedAnnotation final : public IDXVKUserDefinedAnnotation {

  public:

    D3D9UserDefinedAnnotation(D3D9DeviceEx* device);
    ~D3D9UserDefinedAnnotation();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    INT STDMETHODCALLTYPE BeginEvent(
            D3DCOLOR                Color,
            LPCWSTR                 Name);

    INT STDMETHODCALLTYPE EndEvent();

    void STDMETHODCALLTYPE SetMarker(
            D3DCOLOR                Color,
            LPCWSTR                 Name);

    BOOL STDMETHODCALLTYPE GetStatus();

  private:

    D3D9DeviceEx* m_container;

  };

}