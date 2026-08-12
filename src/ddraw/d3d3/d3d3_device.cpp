#include "d3d3_device.h"

#include "../d3d_common_material.h"
#include "../d3d_common_texture.h"
#include "../ddraw_common_interface.h"

#include "d3d3_execute_buffer.h"

#include "../ddraw/ddraw_surface.h"

#include "../d3d6/d3d6_device.h"
#include "../d3d5/d3d5_device.h"
#include "../d3d_process_vertices.h"

#include <algorithm>

namespace dxvk {

  D3D3Device::D3D3Device(
        D3DCommonDevice* commonD3DDevice,
        DDrawSurface* pParent,
        GUID deviceGUID,
        const d3d9::D3DPRESENT_PARAMETERS* pParams9,
        Com<d3d9::IDirect3DDevice9>&& pDevice9,
        DWORD CreationFlags9)
    : DDrawChildObject<DDrawSurface, IDirect3DDevice>(pParent)
    , m_commonD3DDevice ( commonD3DDevice )
    , m_multithread ( CreationFlags9 & D3DCREATE_MULTITHREADED )
    , m_rt ( pParent ) {
    // This isn't technically possible, but play it safe
    if (unlikely(m_parent == nullptr))
      throw DxvkError("D3D3Device: ERROR! Failed to retrieve the parent surface!");

    m_commonIntf = m_parent->GetCommonInterface();

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();
    // Retrieve and cache the device capabilities
    m_desc = GetD3D3BaseCaps(d3dOptions);
    ApplyD3D3DeviceCaps(&m_desc, deviceGUID);

    d3d9::IDirect3DDevice9* device9;

    if (likely(m_commonD3DDevice == nullptr)) {
      m_commonD3DDevice = new D3DCommonDevice(m_commonIntf, deviceGUID, pParams9, CreationFlags9);

      m_commonD3DDevice->SetD3D9Device(std::move(pDevice9));
      device9 = m_commonD3DDevice->GetD3D9Device();

      if (unlikely(d3dOptions->emulateFSAA == FSAAEmulation::Forced)) {
        Logger::warn("D3D3Device: Force enabling AA");
        device9->SetRenderState(d3d9::D3DRS_MULTISAMPLEANTIALIAS, TRUE);
      }
    } else {
      device9 = m_commonD3DDevice->GetD3D9Device();
      // Very important, otherwise the depth stencil isn't dirtied on draws
      m_ds = m_rt->GetAttachedDepthStencil();
    }

    // Get the bridge interface to D3D9
    if (unlikely(FAILED(device9->QueryInterface(__uuidof(IDxvkLegacyD3DDeviceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D3Device: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    if (unlikely(!m_commonD3DDevice->GetTotalTextureMemory()))
      m_commonD3DDevice->SetTotalTextureMemory(m_bridge->DetermineInitialTextureMemory());

    // Update D3D9 legacy light state
    m_bridge->SetLegacyLightsState(true);

    // Update D3D9 alternate pixel center
    m_bridge->SetAlternatePixelCenter(d3dOptions->alternatePixelCenter == AlternatePixelCenter::Enabled);

    if (m_commonD3DDevice->GetOrigin() == nullptr)
      m_commonD3DDevice->SetOrigin(this);

    m_commonD3DDevice->SetD3D3Device(this);

    m_stats.dwSize = sizeof(D3DSTATS);
  }

  D3D3Device::~D3D3Device() {
    // Dissasociate every bound viewport from this device
    for (auto viewport : m_viewports) {
      viewport->GetCommonViewport()->SetD3D3Device(nullptr);
    }

    if (m_commonD3DDevice->GetD3D3Device() == this)
      m_commonD3DDevice->SetD3D3Device(nullptr);

    if (m_commonD3DDevice->GetOrigin() == this)
      m_commonD3DDevice->SetOrigin(nullptr);
  }

  // Interlocked refcount with the origin device
  ULONG STDMETHODCALLTYPE D3D3Device::AddRef() {
    IUnknown* origin = m_commonD3DDevice->GetOrigin();
    if (unlikely(origin != nullptr && origin != this)) {
      return origin->AddRef();
    } else {
      return ComObjectClamp::AddRef();
    }
  }

  // Interlocked refcount with the origin device
  ULONG STDMETHODCALLTYPE D3D3Device::Release() {
    IUnknown* origin = m_commonD3DDevice->GetOrigin();
    if (unlikely(origin != nullptr && origin != this)) {
      return origin->Release();
    } else {
      return ComObjectClamp::Release();
    }
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DDevice2))) {
      if (likely(m_commonD3DDevice->GetD3D5Device() != nullptr))
        return m_commonD3DDevice->GetD3D5Device()->QueryInterface(riid, ppvObject);

      // A D3D3 device shouldn't be able to create a D3D5 device
      // if it doesn't previously exist as a parent/origin device
      return E_NOINTERFACE;
    }
    if (unlikely(riid == __uuidof(IDirect3DDevice3))) {
      if (likely(m_commonD3DDevice->GetD3D6Device() != nullptr))
        return m_commonD3DDevice->GetD3D6Device()->QueryInterface(riid, ppvObject);

      // A D3D3 device shouldn't be able to create a D3D6 device
      // if it doesn't previously exist as a parent/origin device
      return E_NOINTERFACE;
    }
    // None of the below work when the object originates from a higher version device
    if (m_commonD3DDevice->GetOrigin() == this) {
      // If the device is created from a surface, then this must return E_NOINTERFACE
      if (unlikely(riid == __uuidof(IDirect3DDevice)))
        return E_NOINTERFACE;

      // Apparently all of these should return a reference to the parent surface
      if (riid == IID_IDirect3DHALDevice  || riid == IID_IDirect3DRGBDevice  ||
          riid == IID_IDirect3DMMXDevice  || riid == IID_IDirect3DRampDevice ||
          riid == IID_WineD3DDevice) {
        *ppvObject = ref(m_parent);
        return S_OK;
      }

      // IDirect3DDevice is quite different than any of the later device interfaces,
      // because it will also expose what is available on its parent surface. An easy
      // way to handle it, since the surface is its parent, is to forward the calls.
      return m_parent->QueryInterface(riid, ppvObject);
    }

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3DDevice))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D3Device::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc) {
    if (unlikely(hal_desc == nullptr || hel_desc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!IsValidD3DDeviceDescSize(hal_desc->dwSize)
              || !IsValidD3DDeviceDescSize(hel_desc->dwSize)))
      return DDERR_INVALIDPARAMS;

    D3DDEVICEDESC3 desc_HAL = m_desc;
    D3DDEVICEDESC3 desc_HEL = m_desc;

    const GUID deviceGUID = m_commonD3DDevice->GetDeviceGUID();

    if (deviceGUID == IID_IDirect3DRGBDevice) {
      desc_HAL.dwFlags = 0;
      desc_HAL.dcmColorModel = 0;
      // Some applications apparently care about HAL texture caps
      desc_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                          & ~D3DPTEXTURECAPS_POW2
                                          & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
      desc_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                          & ~D3DPTEXTURECAPS_POW2
                                          & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    } else if (deviceGUID == IID_IDirect3DHALDevice) {
      desc_HEL.dcmColorModel = 0;
      desc_HEL.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT
                          & ~D3DDEVCAPS_DRAWPRIMITIVES2
                          & ~D3DDEVCAPS_DRAWPRIMITIVES2EX;
    } else {
      Logger::warn("D3D3Device::GetCaps: Unhandled device type");
    }

    memcpy(hal_desc, &desc_HAL, hal_desc->dwSize);
    memcpy(hel_desc, &desc_HEL, hel_desc->dwSize);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::SwapTextureHandles(IDirect3DTexture *tex1, IDirect3DTexture *tex2) {
    D3DDeviceLock lock = LockDevice();

    D3D3Texture* texture1 = static_cast<D3D3Texture*>(tex1);
    D3D3Texture* texture2 = static_cast<D3D3Texture*>(tex2);

    D3DCommonTexture* commonTex1 = texture1->GetCommonTexture();
    D3DCommonTexture* commonTex2 = texture2->GetCommonTexture();

    const D3DTEXTUREHANDLE handle1 = commonTex1->GetTextureHandle();
    const D3DTEXTUREHANDLE handle2 = commonTex2->GetTextureHandle();

    DDrawCommonInterface::ReleaseTextureHandle(handle1);
    DDrawCommonInterface::ReleaseTextureHandle(handle2);

    commonTex1->SetTextureHandle(handle2);
    commonTex2->SetTextureHandle(handle1);

    DDrawCommonInterface::EmplaceTexture(commonTex1, handle2);
    DDrawCommonInterface::EmplaceTexture(commonTex2, handle1);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetStats(D3DSTATS *stats) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(stats == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(stats->dwSize != sizeof(D3DSTATS)))
      return DDERR_INVALIDPARAMS;

    *stats = m_stats;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::AddViewport(IDirect3DViewport *viewport) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);

    auto it = std::find(m_viewports.begin(), m_viewports.end(), d3d3Viewport);
    if (unlikely(it != m_viewports.end())) {
      Logger::warn("D3D3Device::AddViewport: Pre-existing viewport found");
    } else {
      m_viewports.push_back(d3d3Viewport);
      d3d3Viewport->GetCommonViewport()->SetD3D3Device(this);
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::DeleteViewport(IDirect3DViewport *viewport) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);

    auto it = std::find(m_viewports.begin(), m_viewports.end(), d3d3Viewport);
    if (likely(it != m_viewports.end())) {
      m_viewports.erase(it);
      d3d3Viewport->GetCommonViewport()->SetD3D3Device(nullptr);
      // Clear the current viewport if it is deleted from the device
      if (m_currentViewport.ptr() == d3d3Viewport)
        m_currentViewport = nullptr;
    } else {
      Logger::warn("D3D3Device::DeleteViewport: Viewport not found");
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::NextViewport(IDirect3DViewport *lpDirect3DViewport, IDirect3DViewport **lplpAnotherViewport, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(lplpAnotherViewport == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpAnotherViewport);

    if (flags & D3DNEXT_HEAD) {
      if (likely(m_viewports.size() > 0))
        *lplpAnotherViewport = m_viewports.front().ref();
    } else if (flags & D3DNEXT_NEXT) {
      if (unlikely(lpDirect3DViewport == nullptr))
        return DDERR_INVALIDPARAMS;

      if (likely(m_viewports.size() > 0))
        Logger::warn("D3D3Device::NextViewport: Unimplemented D3DNEXT_NEXT flag");
    } else if (flags & D3DNEXT_TAIL) {
      if (likely(m_viewports.size() > 0))
        *lplpAnotherViewport = m_viewports.back().ref();
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK cb, void *ctx) {
    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    DDSURFACEDESC textureFormat = { };
    textureFormat.dwSize  = sizeof(DDSURFACEDESC);
    textureFormat.dwFlags = DDSD_CAPS | DDSD_PIXELFORMAT;
    textureFormat.ddsCaps.dwCaps = DDSCAPS_TEXTURE;

    // Note: The list of formats exposed in D3D3 is restricted to the below

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_X1R5G5B5);
    HRESULT hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_A1R5G5B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    // D3DFMT_X4R4G4B4 is not supported by D3D3
    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_A4R4G4B4);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_R5G6B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_X8R8G8B8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_A8R8G8B8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    if (d3dOptions->supportR3G3B2) {
      textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_R3G3B2);
      hr = cb(&textureFormat, ctx);
      if (unlikely(hr != D3DENUMRET_OK))
        return D3D_OK;
    }

    // Not supported in D3D9, but some games may use it
    /*textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_P8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;*/

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::BeginScene() {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(m_commonD3DDevice->IsInScene()))
      return D3DERR_SCENE_IN_SCENE;

    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->BeginScene();
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonD3DDevice->SetInScene(true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::EndScene() {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!m_commonD3DDevice->IsInScene()))
      return D3DERR_SCENE_NOT_IN_SCENE;

    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->EndScene();
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonD3DDevice->SetInScene(false);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetDirect3D(IDirect3D **d3d) {
    if (unlikely(d3d == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(d3d);

    // D3D3 is "special", and a D3D3 interface may not exist
    // at all unless it's explicitly created before this call
    D3D3Interface* d3d3Intf = m_commonIntf->GetOrCreateD3D3Interface();
    // Shouldn't happen under normal circumstances, but validate just in case
    if (unlikely(d3d3Intf == nullptr)) {
      Logger::err("D3D3Device::GetDirect3D: Found no valid D3D interface");
      return DDERR_NOTFOUND;
    }

    *d3d = ref(d3d3Intf);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::Initialize(IDirect3D *d3d, GUID *lpGUID, D3DDEVICEDESC *desc) {
    if (unlikely(d3d == nullptr))
      return DDERR_INVALIDPARAMS;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::CreateExecuteBuffer(D3DEXECUTEBUFFERDESC *desc, IDirect3DExecuteBuffer **buffer, IUnknown *pkOuter) {
    if (unlikely(desc == nullptr || buffer == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(desc->dwSize != sizeof(D3DEXECUTEBUFFERDESC)))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(buffer);

    *buffer = ref(new D3D3ExecuteBuffer(this, desc));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::Execute(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(buffer == nullptr || viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    D3D3ExecuteBuffer* d3d3ExecuteBuffer = static_cast<D3D3ExecuteBuffer*>(buffer);

    std::vector<uint8_t> executeBuffer = d3d3ExecuteBuffer->GetBuffer();
    if (unlikely(executeBuffer.size() == 0))
      return DDERR_INVALIDPARAMS;

    d3d3ExecuteBuffer->SetExecutedState(true);

    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);

    if (unlikely(m_currentViewport != d3d3Viewport)) {
      D3DCommonViewport* commonViewport = d3d3Viewport->GetCommonViewport();

      // Validate that the viewport is attached to this (common) device
      if (unlikely(m_commonD3DDevice != commonViewport->GetCommonD3DDevice()))
        return DDERR_INVALIDPARAMS;

      if (likely(m_currentViewport != nullptr)) {
        D3DCommonViewport* currentCommonViewport = m_currentViewport->GetCommonViewport();
        // Shouldn't be necessary, but play it safe, as there is some potential
        // for improper behavior if we skip deactivation during D3D5/6 interop
        if (currentCommonViewport->HasLights())
          currentCommonViewport->DeactivateLights();
        currentCommonViewport->SetIsCurrentViewport(false);
      }

      m_currentViewport = d3d3Viewport;

      commonViewport->SetIsCurrentViewport(true);
      if (likely(commonViewport->IsViewportSet()))
        commonViewport->ApplyViewport();
    }

    D3DEXECUTEDATA* executeData = d3d3ExecuteBuffer->GetExecuteDataInternal();

    uint8_t* buf = executeBuffer.data();
    D3DVERTEX* vertexBuffer = reinterpret_cast<D3DVERTEX*>(buf + executeData->dwVertexOffset);
    D3DTLVERTEX* hVertexBuffer = reinterpret_cast<D3DTLVERTEX*>(buf + executeData->dwHVertexOffset);

    uint8_t* ptr = buf + executeData->dwInstructionOffset;

    // We can't rely on executeData->dwInstructionLength being correct.
    while (true) {
      D3DINSTRUCTION* instruction = reinterpret_cast<D3DINSTRUCTION*>(ptr);
      ptr += sizeof(D3DINSTRUCTION);
      uint8_t* operation = ptr;

      if (instruction->bOpcode == D3DOP_EXIT)
        break;

      switch (instruction->bOpcode) {
        case D3DOP_BRANCHFORWARD: {
          D3DBRANCH* branch = reinterpret_cast<D3DBRANCH*>(operation);
          for (uint16_t i = 0; i < instruction->wCount; i++) {
            const D3DBRANCH& b = branch[i];

            bool masked = (executeData->dsStatus.dwStatus & b.dwMask) == b.dwValue;
            if (b.bNegate) {
              masked = !masked;
            }

            if (masked && b.dwOffset) {
              ptr = reinterpret_cast<uint8_t*>(instruction) + branch->dwOffset;
              break;
            }

            ptr += instruction->bSize;
          }

          break;
        }
        case D3DOP_LINE: {
          D3DLINE* line = reinterpret_cast<D3DLINE*>(operation);
          DrawLineInternal(line, instruction->wCount, executeData->dwVertexCount, hVertexBuffer);

          ptr += instruction->bSize * instruction->wCount;
          break;
        }
        case D3DOP_POINT: {
          D3DPOINT* point = reinterpret_cast<D3DPOINT*>(operation);
          DrawPointInternal(point, instruction->wCount, executeData->dwVertexCount, hVertexBuffer);

          ptr += instruction->bSize * instruction->wCount;
          break;
        }
        case D3DOP_TRIANGLE: {
          D3DTRIANGLE* triangle = reinterpret_cast<D3DTRIANGLE*>(operation);
          DrawTriangleInternal(triangle, instruction->wCount, executeData->dwVertexCount, hVertexBuffer);

          ptr += instruction->bSize * instruction->wCount;
          break;
        }
        case D3DOP_MATRIXLOAD: {
          D3DMATRIXLOAD* matrixLoad = reinterpret_cast<D3DMATRIXLOAD*>(operation);

          for (uint16_t i = 0; i < instruction->wCount; i++) {
            D3DMATRIXLOAD& ml = matrixLoad[i];

            D3DMATRIX srcMatrix;
            HRESULT hr = GetMatrix(ml.hSrcMatrix, &srcMatrix);
            if (unlikely(FAILED(hr))) {
              Logger::warn(str::format("D3D3Device::Execute: D3DOP_MATRIXLOAD failed to retrieve source matrix: ", ml.hSrcMatrix));
              continue;
            }

            hr = SetMatrix(ml.hDestMatrix, &srcMatrix);
            if (unlikely(FAILED(hr)))
              Logger::warn(str::format("D3D3Device::Execute: D3DOP_MATRIXLOAD failed to set matrix to destination: ", ml.hDestMatrix));
          }

          ptr += instruction->bSize * instruction->wCount;
          break;
        }
        case D3DOP_MATRIXMULTIPLY: {
          D3DMATRIXMULTIPLY* matrixMultiply = reinterpret_cast<D3DMATRIXMULTIPLY*>(operation);

          for (uint16_t i = 0; i < instruction->wCount; i++) {
            D3DMATRIXMULTIPLY& mm = matrixMultiply[i];

            D3DMATRIX srcMatrix1;
            HRESULT hr = GetMatrix(mm.hSrcMatrix1, &srcMatrix1);
            if (unlikely(FAILED(hr))) {
              Logger::warn(str::format("D3D3Device::Execute: D3DOP_MATRIXMULTIPLY failed to retrieve first source matrix: ", mm.hSrcMatrix1));
              ZeroMemory(&srcMatrix1, sizeof(srcMatrix1));
              continue;
            }

            D3DMATRIX srcMatrix2;
            hr = GetMatrix(mm.hSrcMatrix2, &srcMatrix2);
            if (unlikely(FAILED(hr))) {
              Logger::warn(str::format("D3D3Device::Execute: D3DOP_MATRIXMULTIPLY failed to retrieve second source matrix: ", mm.hSrcMatrix2));
              continue;
            }

            Matrix4 result = MatrixD3DTo4(&srcMatrix2) * MatrixD3DTo4(&srcMatrix1);
            D3DMATRIX destMatrix = Matrix4ToD3D(&result);

            hr = SetMatrix(mm.hDestMatrix, &destMatrix);
            if (unlikely(FAILED(hr)))
              Logger::warn(str::format("D3D3Device::Execute: D3DOP_MATRIXMULTIPLY failed to set matrix to destination: ", mm.hDestMatrix));
          }

          ptr += instruction->bSize * instruction->wCount;
          break;
        }
        case D3DOP_PROCESSVERTICES: {
          D3DPROCESSVERTICES* processVertices = reinterpret_cast<D3DPROCESSVERTICES*>(operation);

          for (uint16_t i = 0; i < instruction->wCount; i++) {
            D3DPROCESSVERTICES& pv = processVertices[i];
            const DWORD op = pv.dwFlags & D3DPROCESSVERTICES_OPMASK;

            switch (op) {
              case D3DPROCESSVERTICES_COPY: {
                memcpy(&hVertexBuffer[pv.wDest], &vertexBuffer[pv.wStart], sizeof(D3DTLVERTEX) * pv.dwCount);
                break;
              }
              case D3DPROCESSVERTICES_TRANSFORM:
              case D3DPROCESSVERTICES_TRANSFORMLIGHT: {
                // "If the rendering device does not have a material assigned to it, the Direct3D lighting engine is disabled."
                const bool doLighting = op == D3DPROCESSVERTICES_TRANSFORMLIGHT &&
                                        m_commonD3DDevice->GetCurrentMaterialHandle() != 0;

                D3DCommonViewport* commonViewport = m_currentViewport->GetCommonViewport();

                ProcessVerticesData pvData;
                pvData.inData = buf + executeData->dwVertexOffset + pv.wStart * sizeof(D3DVERTEX);
                pvData.inFVF = doLighting ? D3DFVF_VERTEX : D3DFVF_LVERTEX;
                pvData.inStride = sizeof(D3DVERTEX);
                pvData.outData = buf + executeData->dwHVertexOffset + pv.wDest * sizeof(D3DTLVERTEX);
                pvData.outFVF = D3DFVF_TLVERTEX;
                pvData.outStride = sizeof(D3DTLVERTEX);
                pvData.vertexCount = pv.dwCount;
                pvData.correction = commonViewport->GetLegacyProjectionMatrix(0);
                pvData.dsStatus = &executeData->dsStatus;
                pvData.doLighting = doLighting;
                pvData.doClipping = (flags & D3DEXECUTE_CLIPPED) && !(flags & D3DEXECUTE_UNCLIPPED);
                pvData.doNotCopyData = pv.dwFlags & D3DPROCESSVERTICES_NOCOLOR;
                pvData.doExtents = pv.dwFlags & D3DPROCESSVERTICES_UPDATEEXTENTS;
                pvData.isLegacy = true;

                std::vector<d3d9::D3DLIGHT9> lights9;
                if (doLighting) {
                  commonViewport->GetD3D9ActiveLights(&lights9);
                  pvData.lights = &lights9;
                } else {
                  pvData.lights = nullptr;
                }

                ProcessVerticesSW(device9, m_commonIntf->GetOptions(), &pvData);

                break;
              }
            }

            m_stats.dwVerticesProcessed += pv.dwCount;
          }

          ptr += instruction->bSize * instruction->wCount;
          break;
        }
        case D3DOP_SPAN: {
          D3DSPAN* span = reinterpret_cast<D3DSPAN*>(operation);
          DrawSpanInternal(span, instruction->wCount, executeData->dwVertexCount, hVertexBuffer);

          ptr += instruction->bSize * instruction->wCount;
          break;
        }
        case D3DOP_STATELIGHT: {
          D3DSTATE* state = reinterpret_cast<D3DSTATE*>(operation);

          for (uint16_t i = 0; i < instruction->wCount; i++) {
            const D3DSTATE& s = state[i];
            SetLightStateInternal(s.dlstLightStateType, s.dwArg[0]);
          }

          ptr += instruction->bSize * instruction->wCount;
          break;
        }
        case D3DOP_STATERENDER: {
          D3DSTATE* state = reinterpret_cast<D3DSTATE*>(operation);

          for (uint16_t i = 0; i < instruction->wCount; i++) {
            const D3DSTATE& s = state[i];
            SetRenderStateInternal(s.drstRenderStateType, s.dwArg[0]);
          }

          ptr += instruction->bSize * instruction->wCount;
          break;
        }
        case D3DOP_STATETRANSFORM: {
          D3DSTATE* state = reinterpret_cast<D3DSTATE*>(operation);
          D3DMATRIX matrix;

          for (uint16_t i = 0; i < instruction->wCount; i++) {
            const D3DSTATE& s = state[i];

            if (unlikely(s.dwArg[0] == 0))
              continue;

            HRESULT hr = GetMatrix(s.dwArg[0], &matrix);
            if (unlikely(FAILED(hr)))
              continue;

            hr = device9->SetTransform(ConvertTransformState(s.dtstTransformStateType), &matrix);
            if (likely(SUCCEEDED(hr))) {
              if (s.dtstTransformStateType == D3DTRANSFORMSTATE_WORLD) {
                m_worldHandle = s.dwArg[0];
              } else if (s.dtstTransformStateType == D3DTRANSFORMSTATE_VIEW) {
                m_viewHandle = s.dwArg[0];
              } else if (s.dtstTransformStateType == D3DTRANSFORMSTATE_PROJECTION) {
                m_projectionHandle = s.dwArg[0];
              }
            } else {
              Logger::warn("D3D3Device::Execute: Failed to set D3D9 transform");
            }
          }

          ptr += instruction->bSize * instruction->wCount;
          break;
        }
        case D3DOP_SETSTATUS: {
          D3DSTATUS* status = reinterpret_cast<D3DSTATUS*>(operation);
          for (uint16_t i = 0; i < instruction->wCount; i++) {
            executeData->dsStatus = status[i];
          }

          ptr += instruction->bSize * instruction->wCount;
          break;
        }
        case D3DOP_TEXTURELOAD: {
          D3DTEXTURELOAD* textureLoad = reinterpret_cast<D3DTEXTURELOAD*>(operation);
          TextureLoadInternal(textureLoad, instruction->wCount);

          ptr += instruction->bSize * instruction->wCount;
          break;
        }
        default:
          Logger::err(str::format("D3D3Device::Execute: Unknown opcode encountered: ", static_cast<uint32_t>(instruction->bOpcode)));
          ptr += instruction->bSize * instruction->wCount;
          break;
      }
    }

    d3d3ExecuteBuffer->SetExecutedState(false);

    return D3D_OK;
  }

  // Equivalent of the later ZVISIBLE render state
  HRESULT STDMETHODCALLTYPE D3D3Device::Pick(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags, D3DRECT *rect) {
    D3DDeviceLock lock = LockDevice();

    Logger::warn("!!! D3D3Device::Pick: Stub");

    if (unlikely(buffer == nullptr || viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3ExecuteBuffer* d3d3ExecuteBuffer = static_cast<D3D3ExecuteBuffer*>(buffer);

    if (unlikely(d3d3ExecuteBuffer->IsLocked()))
      return D3DERR_EXECUTE_LOCKED;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetPickRecords(DWORD *count, D3DPICKRECORD *records) {
    D3DDeviceLock lock = LockDevice();

    Logger::warn("!!! D3D3Device::GetPickRecords: Stub");

    if (unlikely(count == nullptr && records == nullptr))
      return DDERR_INVALIDPARAMS;

    if (count != nullptr)
      *count = 0;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::CreateMatrix(D3DMATRIXHANDLE *matrix) {
    D3DDeviceLock lock = LockDevice();

    m_matrixHandle++;
    m_matrices.emplace(std::piecewise_construct,
                       std::forward_as_tuple(m_matrixHandle),
                       std::forward_as_tuple(D3DMATRIX()));

    *matrix = m_matrixHandle;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::SetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(matrix == nullptr))
      return DDERR_INVALIDPARAMS;

    auto matrixIter = m_matrices.find(handle);

    if (likely(matrixIter != m_matrices.end())) {
      matrixIter->second = *matrix;
    } else {
      Logger::warn("D3D3Device::SetMatrix: Matrix not found");
      return DDERR_INVALIDPARAMS;
    }

    // Update D3D9 transforms if the updated matrix is in use
    if (m_worldHandle == handle) {
      HRESULT hr = m_commonD3DDevice->GetD3D9Device()->SetTransform(ConvertTransformState(D3DTRANSFORMSTATE_WORLD), matrix);
      if (unlikely(FAILED(hr)))
        Logger::warn("D3D3Device::SetMatrix: Failed to update D3D9 world transform");
    }
    if (m_viewHandle == handle) {
      HRESULT hr = m_commonD3DDevice->GetD3D9Device()->SetTransform(ConvertTransformState(D3DTRANSFORMSTATE_VIEW), matrix);
      if (unlikely(FAILED(hr)))
        Logger::warn("D3D3Device::SetMatrix: Failed to update D3D9 view transform");
    }
    if (m_projectionHandle == handle) {
      HRESULT hr = m_commonD3DDevice->GetD3D9Device()->SetTransform(ConvertTransformState(D3DTRANSFORMSTATE_PROJECTION), matrix);
      if (unlikely(FAILED(hr)))
        Logger::warn("D3D3Device::SetMatrix: Failed to update D3D9 projection transform");
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(matrix == nullptr))
      return DDERR_INVALIDPARAMS;

    auto matrixIter = m_matrices.find(handle);

    if (likely(matrixIter != m_matrices.end())) {
      *matrix = matrixIter->second;
    } else {
      Logger::warn(str::format("D3D3Device::GetMatrix: Matrix not found: ", handle));
      return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::DeleteMatrix(D3DMATRIXHANDLE D3DMatHandle) {
    D3DDeviceLock lock = LockDevice();

    auto matrixIter = m_matrices.find(D3DMatHandle);

    if (likely(matrixIter != m_matrices.end())) {
      m_matrices.erase(matrixIter);
    } else {
      Logger::warn("D3D3Device::DeleteMatrix: Matrix not found");
      return DDERR_INVALIDPARAMS;
    }

    if (m_worldHandle == D3DMatHandle)
      m_worldHandle = 0;
    if (m_viewHandle == D3DMatHandle)
      m_viewHandle = 0;
    if (m_projectionHandle == D3DMatHandle)
      m_projectionHandle = 0;

    return D3D_OK;
  }

  // Not called when the device is created from a D3D5/6 device
  void D3D3Device::InitializeDS() {
    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    m_rt->InitializeD3D9RenderTarget();

    m_ds = m_rt->GetAttachedDepthStencil();

    if (m_ds != nullptr) {
      HRESULT hrDS = m_ds->InitializeD3D9DepthStencil();
      if (unlikely(FAILED(hrDS))) {
        Logger::err("D3D3Device::InitializeDS: Failed to initialize D3D9 DS");
      } else {
        const RECT* dsRect = m_ds->GetCommonSurface()->GetFullSurfaceRect();
        Logger::info(str::format("D3D3Device::InitializeDS: Depth stencil: ", dsRect->right, "x", dsRect->bottom));

        HRESULT hrDS9 = device9->SetDepthStencilSurface(m_ds->GetCommonSurface()->GetD3D9Surface());
        if (unlikely(FAILED(hrDS9))) {
          Logger::err("D3D3Device::InitializeDS: Failed to set D3D9 depth stencil");
        } else {
          // This needs to act like an auto depth stencil of sorts, so manually enable z-buffering
          device9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_TRUE);
        }
      }
    } else {
      device9->SetDepthStencilSurface(nullptr);
      // Should be superfluous, but play it safe
      device9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_FALSE);
    }
  }

  void D3D3Device::UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface) {
    if (likely(dirtyRenderTarget))
      m_rt->GetCommonSurface()->DirtyD3D9Surface();

    if (likely(dirtyPrimarySurface)) {
      DDrawCommonSurface* primarySurface = m_commonIntf->GetPrimarySurface();
      // The primary surface can be bound as RT, in which case it will
      // get dirtied twice, but we have no guarantees that will happen
      if (likely(primarySurface != nullptr))
        primarySurface->DirtyD3D9Surface();
    }

    if (likely(dirtyDepthStencil && m_ds != nullptr))
      m_ds->GetCommonSurface()->DirtyD3D9Surface();
  }

  inline void D3D3Device::DDrawDirtySurfaceUpload() {
    // Render target
    m_rt->InitializeOrUploadD3D9();
    // Depth stencil (if present)
    if (likely(m_ds != nullptr))
      m_ds->InitializeOrUploadD3D9();
    // Bound texture(s)
    const D3DTEXTUREHANDLE texHandle = m_commonD3DDevice->GetCurrentTextureHandle();
    if (likely(texHandle != 0)) {
      DDrawSurface* tex = DDrawCommonInterface::GetSurfaceFromTextureHandle(texHandle);
      if (likely(tex != nullptr))
        tex->InitializeOrUploadD3D9();
    }
  }

  inline HRESULT D3D3Device::SetLightStateInternal(D3DLIGHTSTATETYPE dwLightStateType, DWORD dwLightState) {
    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    switch (dwLightStateType) {
      case D3DLIGHTSTATE_MATERIAL: {
        if (unlikely(!dwLightState)) {
          m_commonD3DDevice->SetCurrentMaterialHandle(dwLightState);
          static constexpr d3d9::D3DMATERIAL9 DefaultMaterial9 = { };
          device9->SetMaterial(&DefaultMaterial9);
          return D3D_OK;
        }

        d3d9::D3DMATERIAL9* material9 = D3DCommonInterface::GetCommonMaterialFromHandle(dwLightState)->GetD3D9Material();
        if (unlikely(material9 == nullptr))
          return DDERR_INVALIDPARAMS;

        m_commonD3DDevice->SetCurrentMaterialHandle(dwLightState);
        //Logger::debug(str::format("D3D3Device::SetLightStateInternal: Applying material nr. ", dwLightState, " to D3D9"));
        device9->SetMaterial(material9);

        break;
      }
      case D3DLIGHTSTATE_AMBIENT:
        device9->SetRenderState(d3d9::D3DRS_AMBIENT, dwLightState);
        break;
      case D3DLIGHTSTATE_COLORMODEL:
        if (unlikely(dwLightState != D3DCOLOR_RGB))
          Logger::warn("D3D3Device::SetLightStateInternal: Unsupported D3DLIGHTSTATE_COLORMODEL");
        break;
      case D3DLIGHTSTATE_FOGMODE:
        device9->SetRenderState(d3d9::D3DRS_FOGVERTEXMODE, dwLightState);
        break;
      case D3DLIGHTSTATE_FOGSTART:
        device9->SetRenderState(d3d9::D3DRS_FOGSTART, dwLightState);
        break;
      case D3DLIGHTSTATE_FOGEND:
        device9->SetRenderState(d3d9::D3DRS_FOGEND, dwLightState);
        break;
      case D3DLIGHTSTATE_FOGDENSITY:
        device9->SetRenderState(d3d9::D3DRS_FOGDENSITY, dwLightState);
        break;
      default:
        return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  inline HRESULT D3D3Device::SetRenderStateInternal(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState) {
    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();
    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      //case D3DRENDERSTATE_TEXTUREHANDLE:
      //case D3DRENDERSTATE_ANTIALIAS:
      //case D3DRENDERSTATE_TEXTUREADDRESS:
      //case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
      //case D3DRENDERSTATE_WRAPU:
      //case D3DRENDERSTATE_WRAPV:
      case D3DRENDERSTATE_ZENABLE:
      case D3DRENDERSTATE_FILLMODE:
      case D3DRENDERSTATE_SHADEMODE:
      //case D3DRENDERSTATE_LINEPATTERN:
      //case D3DRENDERSTATE_MONOENABLE:
      //case D3DRENDERSTATE_ROP2:
      //case D3DRENDERSTATE_PLANEMASK:
      case D3DRENDERSTATE_ZWRITEENABLE:
      case D3DRENDERSTATE_ALPHATESTENABLE:
      case D3DRENDERSTATE_LASTPIXEL:
      //case D3DRENDERSTATE_TEXTUREMAG:
      //case D3DRENDERSTATE_TEXTUREMIN:
      case D3DRENDERSTATE_SRCBLEND:
      case D3DRENDERSTATE_DESTBLEND:
      //case D3DRENDERSTATE_TEXTUREMAPBLEND:
      case D3DRENDERSTATE_CULLMODE:
      case D3DRENDERSTATE_ZFUNC:
      case D3DRENDERSTATE_ALPHAREF:
      case D3DRENDERSTATE_ALPHAFUNC:
      //case D3DRENDERSTATE_DITHERENABLE:
      //case D3DRENDERSTATE_BLENDENABLE: // The actual D3DRENDERSTATE_ALPHABLENDENABLE
      case D3DRENDERSTATE_FOGENABLE:
      case D3DRENDERSTATE_SPECULARENABLE:
      //case D3DRENDERSTATE_ZVISIBLE:
      //case D3DRENDERSTATE_SUBPIXEL:
      //case D3DRENDERSTATE_SUBPIXELX:
      //case D3DRENDERSTATE_STIPPLEDALPHA:
      case D3DRENDERSTATE_FOGCOLOR:
      case D3DRENDERSTATE_FOGTABLEMODE:
      case D3DRENDERSTATE_FOGTABLESTART:
      case D3DRENDERSTATE_FOGTABLEEND:
      case D3DRENDERSTATE_FOGTABLEDENSITY:
      //case D3DRENDERSTATE_STIPPLEENABLE:
      //case D3DRENDERSTATE_STIPPLEPATTERN00:
      //case D3DRENDERSTATE_STIPPLEPATTERN01:
      //case D3DRENDERSTATE_STIPPLEPATTERN02:
      //case D3DRENDERSTATE_STIPPLEPATTERN03:
      //case D3DRENDERSTATE_STIPPLEPATTERN04:
      //case D3DRENDERSTATE_STIPPLEPATTERN05:
      //case D3DRENDERSTATE_STIPPLEPATTERN06:
      //case D3DRENDERSTATE_STIPPLEPATTERN07:
      //case D3DRENDERSTATE_STIPPLEPATTERN08:
      //case D3DRENDERSTATE_STIPPLEPATTERN09:
      //case D3DRENDERSTATE_STIPPLEPATTERN10:
      //case D3DRENDERSTATE_STIPPLEPATTERN11:
      //case D3DRENDERSTATE_STIPPLEPATTERN12:
      //case D3DRENDERSTATE_STIPPLEPATTERN13:
      //case D3DRENDERSTATE_STIPPLEPATTERN14:
      //case D3DRENDERSTATE_STIPPLEPATTERN15:
      //case D3DRENDERSTATE_STIPPLEPATTERN16:
      //case D3DRENDERSTATE_STIPPLEPATTERN17:
      //case D3DRENDERSTATE_STIPPLEPATTERN18:
      //case D3DRENDERSTATE_STIPPLEPATTERN19:
      //case D3DRENDERSTATE_STIPPLEPATTERN20:
      //case D3DRENDERSTATE_STIPPLEPATTERN21:
      //case D3DRENDERSTATE_STIPPLEPATTERN22:
      //case D3DRENDERSTATE_STIPPLEPATTERN23:
      //case D3DRENDERSTATE_STIPPLEPATTERN24:
      //case D3DRENDERSTATE_STIPPLEPATTERN25:
      //case D3DRENDERSTATE_STIPPLEPATTERN26:
      //case D3DRENDERSTATE_STIPPLEPATTERN27:
      //case D3DRENDERSTATE_STIPPLEPATTERN28:
      //case D3DRENDERSTATE_STIPPLEPATTERN29:
      //case D3DRENDERSTATE_STIPPLEPATTERN30:
      //case D3DRENDERSTATE_STIPPLEPATTERN31:
        break;

      // Replacement for later implemented SetTexture calls
      case D3DRENDERSTATE_TEXTUREHANDLE: {
        DDrawSurface* surface = nullptr;

        if (likely(dwRenderState != 0)) {
          surface = DDrawCommonInterface::GetSurfaceFromTextureHandle(dwRenderState);
          if (unlikely(surface == nullptr))
            return DDERR_INVALIDPARAMS;
        }

        HRESULT hr = SetTextureInternal(surface, dwRenderState);
        if (unlikely(FAILED(hr)))
          return hr;

        return D3D_OK;
      }

      case D3DRENDERSTATE_ANTIALIAS: {
        const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

        if (likely(d3dOptions->emulateFSAA == FSAAEmulation::Disabled)) {
          if (unlikely(dwRenderState))
            Logger::warn("D3D3Device::SetRenderStateInternal: Device does not expose FSAA emulation");
          return D3D_OK;
        }

        State9 = d3d9::D3DRS_MULTISAMPLEANTIALIAS;
        break;
      }

      case D3DRENDERSTATE_TEXTUREADDRESS:
        device9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, dwRenderState);
        device9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSV, dwRenderState);
        return D3D_OK;

      // Always enabled on later APIs, though default FALSE in D3D3
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        return D3D_OK;

      // Not implemented in DXVK, but forward it anyway
      case D3DRENDERSTATE_WRAPU: {
        DWORD value9 = 0;
        device9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        if (dwRenderState == TRUE) {
          device9->SetRenderState(d3d9::D3DRS_WRAP0, value9 | D3DWRAP_U);
        } else {
          device9->SetRenderState(d3d9::D3DRS_WRAP0, value9 & ~D3DWRAP_U);
        }
        return D3D_OK;
      }

      // Not implemented in DXVK, but forward it anyway
      case D3DRENDERSTATE_WRAPV: {
        DWORD value9 = 0;
        device9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        if (dwRenderState == TRUE) {
          device9->SetRenderState(d3d9::D3DRS_WRAP0, value9 | D3DWRAP_V);
        } else {
          device9->SetRenderState(d3d9::D3DRS_WRAP0, value9 & ~D3DWRAP_V);
        }
        return D3D_OK;
      }

      // TODO: Implement D3DRS_LINEPATTERN - vkCmdSetLineRasterizationModeEXT
      // and advertise support with D3DPRASTERCAPS_PAT once that is done
      case D3DRENDERSTATE_LINEPATTERN:
        static bool s_linePatternErrorShown;

        if (!std::exchange(s_linePatternErrorShown, true))
          Logger::warn("D3D3Device::SetRenderStateInternal: Unimplemented render state D3DRS_LINEPATTERN");

        return D3D_OK;

      case D3DRENDERSTATE_MONOENABLE:
        return D3D_OK;

      case D3DRENDERSTATE_ROP2:
        return D3D_OK;

      // "This render state is not supported by the software rasterizers, and is often ignored by hardware drivers."
      case D3DRENDERSTATE_PLANEMASK:
        return D3D_OK;

      // Docs: "[...]  only the first two (D3DFILTER_NEAREST and
      // D3DFILTER_LINEAR) are valid with D3DRENDERSTATE_TEXTUREMAG."
      case D3DRENDERSTATE_TEXTUREMAG: {
        switch (dwRenderState) {
          case D3DFILTER_NEAREST:
          case D3DFILTER_LINEAR:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MAGFILTER, dwRenderState);
            break;
          default:
            break;
        }
        return D3D_OK;
      }

      case D3DRENDERSTATE_TEXTUREMIN: {
        switch (dwRenderState) {
          case D3DFILTER_NEAREST:
          case D3DFILTER_LINEAR:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, dwRenderState);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_NONE);
            break;
          // "The closest mipmap level is chosen and a point filter is applied."
          case D3DFILTER_MIPNEAREST:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_POINT);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_POINT);
            break;
          // "The closest mipmap level is chosen and a bilinear filter is applied within it."
          case D3DFILTER_MIPLINEAR:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_POINT);
            break;
          // "The two closest mipmap levels are chosen and then a linear
          //  blend is used between point filtered samples of each level."
          case D3DFILTER_LINEARMIPNEAREST:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_POINT);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_LINEAR);
            break;
          // "The two closest mipmap levels are chosen and then combined using a bilinear filter."
          case D3DFILTER_LINEARMIPLINEAR:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_LINEAR);
            break;
          default:
            break;
        }
        return D3D_OK;
      }

      case D3DRENDERSTATE_TEXTUREMAPBLEND:
        // Setting the same blend state won't reset the texture state
        if (m_commonD3DDevice->GetTextureMapBlend() == dwRenderState)
          return D3D_OK;

        m_commonD3DDevice->SetTextureMapBlend(dwRenderState);

        switch (dwRenderState) {
          // "In this mode, the RGB and alpha values of the texture replace
          //  the colors that would have been used with no texturing."
          case D3DTBLEND_DECAL:
          case D3DTBLEND_COPY:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_CURRENT);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            break;
          // "In this mode, the RGB values of the texture are multiplied with the RGB values
          //  that would have been used with no texturing. Any alpha values in the texture
          //  replace the alpha values in the colors that would have been used with no texturing;
          //  if the texture does not contain an alpha component, alpha values at the vertices
          //  in the source are interpolated between vertices."
          case D3DTBLEND_MODULATE:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_MODULATE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_CURRENT);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            break;
          // "In this mode, the RGB and alpha values of the texture are blended with the colors
          //  that would have been used with no texturing, according to the following formulas [...]"
          case D3DTBLEND_DECALALPHA:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_BLENDTEXTUREALPHA);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_CURRENT);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            break;
          // "In this mode, the RGB values of the texture are multiplied with the RGB values that
          //  would have been used with no texturing, and the alpha values of the texture
          //  are multiplied with the alpha values that would have been used with no texturing."
          case D3DTBLEND_MODULATEALPHA:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_MODULATE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_CURRENT);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            break;
          // "Add the Gouraud interpolants to the texture lookup with saturation semantics
          //  (that is, if the color value overflows it is set to the maximum possible value)."
          case D3DTBLEND_ADD:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_ADD);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_CURRENT);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            break;
          // Unsupported
          default:
          case D3DTBLEND_DECALMASK:
          case D3DTBLEND_MODULATEMASK:
            break;
        }

        return D3D_OK;

      // Replaced by D3DRENDERSTATE_ALPHABLENDENABLE
      case D3DRENDERSTATE_BLENDENABLE:
        State9 = d3d9::D3DRS_ALPHABLENDENABLE;
        break;

      // Safe to ignore. Docs state: "Direct3D's retained mode uses this operation as a
      // quick-reject test: it does the z-visible test on the bounding box of a set of
      // primitives and only renders them if it returns TRUE."
      case D3DRENDERSTATE_ZVISIBLE:
        return D3D_OK;

      // Docs state: "Most hardware either doesn't support it (always off) or
      // always supports it (always on).", and "All hardware should be subpixel correct.
      // Some software rasterizers are not subpixel correct because of the performance loss."
      case D3DRENDERSTATE_SUBPIXEL:
      case D3DRENDERSTATE_SUBPIXELX:
        return D3D_OK;

      // Tests have shown age accurate GPUs didn't offer support for stippling at all
      case D3DRENDERSTATE_STIPPLEDALPHA:
        return D3D_OK;

      // Tests have shown age accurate GPUs didn't offer support for stippling at all
      case D3DRENDERSTATE_STIPPLEENABLE:
        return D3D_OK;

      // Tests have shown age accurate GPUs didn't offer support for stippling at all
      case D3DRENDERSTATE_STIPPLEPATTERN00:
      case D3DRENDERSTATE_STIPPLEPATTERN01:
      case D3DRENDERSTATE_STIPPLEPATTERN02:
      case D3DRENDERSTATE_STIPPLEPATTERN03:
      case D3DRENDERSTATE_STIPPLEPATTERN04:
      case D3DRENDERSTATE_STIPPLEPATTERN05:
      case D3DRENDERSTATE_STIPPLEPATTERN06:
      case D3DRENDERSTATE_STIPPLEPATTERN07:
      case D3DRENDERSTATE_STIPPLEPATTERN08:
      case D3DRENDERSTATE_STIPPLEPATTERN09:
      case D3DRENDERSTATE_STIPPLEPATTERN10:
      case D3DRENDERSTATE_STIPPLEPATTERN11:
      case D3DRENDERSTATE_STIPPLEPATTERN12:
      case D3DRENDERSTATE_STIPPLEPATTERN13:
      case D3DRENDERSTATE_STIPPLEPATTERN14:
      case D3DRENDERSTATE_STIPPLEPATTERN15:
      case D3DRENDERSTATE_STIPPLEPATTERN16:
      case D3DRENDERSTATE_STIPPLEPATTERN17:
      case D3DRENDERSTATE_STIPPLEPATTERN18:
      case D3DRENDERSTATE_STIPPLEPATTERN19:
      case D3DRENDERSTATE_STIPPLEPATTERN20:
      case D3DRENDERSTATE_STIPPLEPATTERN21:
      case D3DRENDERSTATE_STIPPLEPATTERN22:
      case D3DRENDERSTATE_STIPPLEPATTERN23:
      case D3DRENDERSTATE_STIPPLEPATTERN24:
      case D3DRENDERSTATE_STIPPLEPATTERN25:
      case D3DRENDERSTATE_STIPPLEPATTERN26:
      case D3DRENDERSTATE_STIPPLEPATTERN27:
      case D3DRENDERSTATE_STIPPLEPATTERN28:
      case D3DRENDERSTATE_STIPPLEPATTERN29:
      case D3DRENDERSTATE_STIPPLEPATTERN30:
      case D3DRENDERSTATE_STIPPLEPATTERN31:
        return D3D_OK;

      // As opposed to D3D7, D3D3 does not error out on
      // unknown or invalid render states.
      default:
        return D3D_OK;
    }

    // This call will never fail
    return device9->SetRenderState(State9, dwRenderState);
  }

  inline void D3D3Device::DrawTriangleInternal(D3DTRIANGLE* triangle, uint16_t count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer) {
    std::vector<D3DTLVERTEX> vertices;

    for (uint16_t i = 0; i < count; i++) {
      const D3DTRIANGLE& t = triangle[i];

      if (t.v1 >= vertexCount || t.v2 >= vertexCount || t.v3 >= vertexCount)
        continue;

      // TODO: Ignoring t.wFlags for now as they are relevant only for wireframe mode?
      // (D3DTRIFLAG_START, D3DTRIFLAG_STARTFLAT(1-29), D3DTRIFLAG_ODD(strip),
      // D3DTRIFLAG_EVEN(fan) and D3DTRIFLAG_EDGEENABLE).

      vertices.push_back(vertexBuffer[t.v1]);
      vertices.push_back(vertexBuffer[t.v2]);
      vertices.push_back(vertexBuffer[t.v3]);
    }

    if (likely(!vertices.empty())) {
      DDrawDirtySurfaceUpload();

      d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

      device9->SetFVF(D3DFVF_TLVERTEX);
      HRESULT hr = device9->DrawPrimitiveUP(
           d3d9::D3DPT_TRIANGLELIST,
           GetPrimitiveCount(D3DPT_TRIANGLELIST, vertices.size()),
           vertices.data(),
           GetFVFSize(D3DFVF_TLVERTEX));

      if (SUCCEEDED(hr)) {
        UpdateSurfaceDirtyTracking(true, true, true);
        m_stats.dwTrianglesDrawn += std::max<DWORD>(vertices.size() / 3, 0u);
      } else {
        Logger::err(str::format("D3D3Device::Execute: D3DOP_TRIANGLE failed to draw vertices: ", vertices.size()));
      }

      vertices.clear();
    }
  }

  inline void D3D3Device::DrawLineInternal(D3DLINE* line, uint16_t count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer) {
    std::vector<D3DTLVERTEX> vertices;

    for (uint16_t i = 0; i < count; i++) {
      const D3DLINE& l = line[i];

      if (l.v1 >= vertexCount || l.v2 >= vertexCount)
        continue;

      vertices.push_back(vertexBuffer[l.v1]);
      vertices.push_back(vertexBuffer[l.v2]);
    }

    if (likely(!vertices.empty())) {
      DDrawDirtySurfaceUpload();

      d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

      device9->SetFVF(D3DFVF_TLVERTEX);
      HRESULT hr = device9->DrawPrimitiveUP(
           d3d9::D3DPT_LINELIST,
           GetPrimitiveCount(D3DPT_LINELIST, vertices.size()),
           vertices.data(),
           GetFVFSize(D3DFVF_TLVERTEX));

      if (SUCCEEDED(hr)) {
        UpdateSurfaceDirtyTracking(true, true, true);
        m_stats.dwLinesDrawn += std::max<DWORD>(vertices.size() / 2, 0u);
      } else {
        Logger::err(str::format("D3D3Device::Execute: D3DOP_LINE failed to draw vertices: ", vertices.size()));
      }

      vertices.clear();
    }
  }

  inline void D3D3Device::DrawPointInternal(D3DPOINT* point, uint16_t count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer) {
    std::vector<D3DTLVERTEX> vertices;

    for (uint16_t i = 0; i < count; i++) {
      const D3DPOINT& p = point[i];

      if (p.wFirst >= vertexCount)
        continue;

      for (DWORD x = 0; x < std::min(static_cast<DWORD>(p.wCount), vertexCount - p.wFirst); x++) {
        vertices.push_back(vertexBuffer[p.wFirst + x]);
      }
    }

    if (likely(!vertices.empty())) {
      DDrawDirtySurfaceUpload();

      d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

      device9->SetFVF(D3DFVF_TLVERTEX);
      HRESULT hr = device9->DrawPrimitiveUP(
           d3d9::D3DPT_POINTLIST,
           GetPrimitiveCount(D3DPT_POINTLIST, vertices.size()),
           vertices.data(),
           GetFVFSize(D3DFVF_TLVERTEX));

      if (SUCCEEDED(hr)) {
        UpdateSurfaceDirtyTracking(true, true, true);
        m_stats.dwPointsDrawn += static_cast<DWORD>(vertices.size());
      } else {
        Logger::err(str::format("D3D3Device::Execute: D3DOP_POINT failed to draw vertices: ", vertices.size()));
      }

      vertices.clear();
    }
  }

  inline void D3D3Device::DrawSpanInternal(D3DSPAN* span, uint16_t count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer) {
    std::vector<D3DTLVERTEX> vertices;

    for (uint16_t i = 0; i < count; i++) {
      const D3DSPAN& s = span[i];

      if (s.wFirst >= vertexCount)
        continue;

      for (DWORD x = 0; x < std::min(static_cast<DWORD>(s.wCount), vertexCount - s.wFirst); x++) {
        vertices.push_back(vertexBuffer[s.wFirst + x]);
      }
    }

    if (likely(!vertices.empty())) {
      DDrawDirtySurfaceUpload();

      d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

      device9->SetFVF(D3DFVF_TLVERTEX);
      HRESULT hr = device9->DrawPrimitiveUP(
           d3d9::D3DPT_LINESTRIP,
           GetPrimitiveCount(D3DPT_LINESTRIP, vertices.size()),
           vertices.data(),
           GetFVFSize(D3DFVF_TLVERTEX));

      if (SUCCEEDED(hr)) {
        UpdateSurfaceDirtyTracking(true, true, true);
        m_stats.dwSpansDrawn += std::max<DWORD>(vertices.size() - 1, 0u);
      } else {
        Logger::err(str::format("D3D3Device::Execute: D3DOP_SPAN failed to draw vertices: ", vertices.size()));
      }

      vertices.clear();
    }
  }

  inline void D3D3Device::TextureLoadInternal(D3DTEXTURELOAD* textureLoad, uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
      const D3DTEXTURELOAD& tl = textureLoad[i];

      DDrawSurface* destSurf = DDrawCommonInterface::GetSurfaceFromTextureHandle(tl.hDestTexture);
      DDrawSurface* srcSurf = DDrawCommonInterface::GetSurfaceFromTextureHandle(tl.hSrcTexture);
      if (destSurf != nullptr && srcSurf != nullptr) {
        destSurf->GetD3D3Texture()->Load(srcSurf->GetD3D3Texture());
      } else {
        Logger::warn("D3D3Device::Execute: D3DOP_TEXTURELOAD source or/and destination texture is null");
      }
    }
  }

  inline HRESULT D3D3Device::SetTextureInternal(DDrawSurface* surface, DWORD textureHandle) {
    HRESULT hr;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    // Unbinding texture stages
    if (surface == nullptr) {
      hr = device9->SetTexture(0, nullptr);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D3Device::SetTextureInternal: Failed to unbind D3D9 texture");
        return hr;
      }

      if (likely(m_commonD3DDevice->GetCurrentTextureHandle() != 0))
        m_commonD3DDevice->SetCurrentTextureHandle(0);

      return D3D_OK;
    }

    DDrawCommonSurface* commonSurface = surface->GetCommonSurface();

    // If textures have been used on a different device, they
    // will get their D3D9 object reinitialized at this point
    if (unlikely(commonSurface->GetCommonD3DDevice() != m_commonD3DDevice.ptr()))
      commonSurface->DirtyDDrawSurface();

    hr = surface->InitializeOrUploadD3D9();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D3Device::SetTextureInternal: Failed to initialize/upload D3D9 texture");
      return hr;
    }

    // Don't fast skip, since color key might change
    //if (unlikely(m_commonD3DDevice->GetCurrentTextureHandle() == textureHandle))
      //return D3D_OK;

    d3d9::IDirect3DTexture9* tex9 = commonSurface->GetD3D9Texture();

    if (likely(tex9 != nullptr)) {
      hr = device9->SetTexture(0, tex9);
      if (unlikely(FAILED(hr))) {
        Logger::warn("D3D3Device::SetTextureInternal: Failed to bind D3D9 texture");
        return hr;
      }

      // "Any alpha values in the texture replace the alpha values in the colors that would
      //  have been used with no texturing; if the texture does not contain an alpha component,
      //  alpha values at the vertices in the source are interpolated between vertices."
      if (m_commonD3DDevice->GetTextureMapBlend() == D3DTBLEND_MODULATE) {
        const DWORD textureOp = commonSurface->IsAlphaFormat() ? D3DTOP_SELECTARG1 : D3DTOP_SELECTARG2;
        device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP, textureOp);
      }

      // D3D3 enables color key transparency globally
      const bool validColorKey = commonSurface->HasValidColorKey();
      m_bridge->SetColorKeyState(validColorKey);
      if (validColorKey) {
        DDCOLORKEY normalizedColorKey = commonSurface->GetColorKeyNormalized();
        m_bridge->SetColorKey(normalizedColorKey.dwColorSpaceLowValue,
                              normalizedColorKey.dwColorSpaceHighValue);
      }
    } else {
      Logger::err("D3D3Device::SetTextureInternal: Found no valid D3D9 texture");
    }

    m_commonD3DDevice->SetCurrentTextureHandle(textureHandle);

    return D3D_OK;
  }

}