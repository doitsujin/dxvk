#include "ddraw_include.h"

#include "ddraw_class_factory.h"

#include "ddraw_clipper.h"
#include "ddraw/ddraw_interface.h"
#include "ddraw7/ddraw7_interface.h"

#include <vector>

namespace dxvk {

  //Logger Logger::s_instance("ddraw.log");

  HMODULE GetProxiedDDrawModule() {
    static HMODULE hDDraw = nullptr;

    if (unlikely(hDDraw == nullptr)) {
      // Try to load ddraw_.dll from the current path first
      hDDraw = ::LoadLibraryA("ddraw_.dll");

      if (hDDraw == nullptr) {
        // Determine the system directory path
        char loadPath[MAX_PATH] = { };
        UINT returnLength = ::GetSystemDirectoryA(loadPath, MAX_PATH);
        if (unlikely(!returnLength))
          return nullptr;

        strcat(loadPath, "\\ddraw.dll");
        hDDraw = ::LoadLibraryA(loadPath);

        if (likely(hDDraw != nullptr)) {
          Logger::debug("GetProxiedDDrawModule: Loaded ddraw.dll from system path");

          // We still need to validate that we've not actually loaded D7VK again,
          // because that has the potential to liveloop any subsequent call
          const DWORD versionInfoSize = ::GetFileVersionInfoSizeA(loadPath, nullptr);
          if (unlikely(versionInfoSize == 0))
            return nullptr;

          std::vector<BYTE> versionInfo(versionInfoSize);
          const BOOL result = ::GetFileVersionInfoA(loadPath, 0, versionInfoSize, versionInfo.data());
          if (unlikely(!result))
            return nullptr;

          void* pvData = nullptr;
          UINT iLenData = 0u;
          // DXVK/D7VK will use the English UK encoding, so simply skip
          // the check if nothing is returned (pvData will be nullptr)
          ::VerQueryValueA(versionInfo.data(), "\\StringFileInfo\\080904B0\\ProductName", &pvData, &iLenData);

          if (pvData != nullptr) {
            Logger::debug(str::format("GetProxiedDDrawModule: ProductName: ", static_cast<char*>(pvData)));

            if (unlikely(!strcmp(static_cast<char*>(pvData), "DXVK"))) {
              Logger::err("GetProxiedDDrawModule: Loaded proxy ddraw.dll is also D7VK!");
              // FreeLibrary won't work properly here since we've loaded
              // ourselves, and, well, we're already loaded, now aren't we?
              // The program will most likely crash at this point, or soon
              // enough, so no point in worrying about it too much though.
              hDDraw = nullptr;
              return nullptr;
            }
          }
        }
      } else {
        Logger::debug("GetProxiedDDrawModule: Loaded ddraw_.dll");
      }
    }

    return hDDraw;
  }

  HRESULT CreateDirectDrawEx(GUID *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown *pUnkOuter) {
    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDD);

    if (unlikely(iid != __uuidof(IDirectDraw7)))
      return DDERR_INVALIDPARAMS;

    typedef HRESULT (__stdcall *DirectDrawCreateEx_t)(GUID *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown *pUnkOuter);
    static DirectDrawCreateEx_t ProxiedDirectDrawCreateEx = nullptr;

    try {
      if (unlikely(ProxiedDirectDrawCreateEx == nullptr)) {
        HMODULE hDDraw = GetProxiedDDrawModule();

        if (unlikely(!hDDraw)) {
          Logger::err("CreateDirectDrawEx: Failed to load proxied ddraw.dll");
          return DDERR_GENERIC;
        }

        ProxiedDirectDrawCreateEx = reinterpret_cast<DirectDrawCreateEx_t>(::GetProcAddress(hDDraw, "DirectDrawCreateEx"));

        if (unlikely(ProxiedDirectDrawCreateEx == nullptr)) {
          Logger::err("CreateDirectDrawEx: Failed GetProcAddress");
          return DDERR_GENERIC;
        }
      }

      LPVOID lplpDDProxied = nullptr;
      HRESULT hr = ProxiedDirectDrawCreateEx(lpGUID, &lplpDDProxied, iid, pUnkOuter);
      if (unlikely(FAILED(hr)))
        return hr;

      Com<IDirectDraw7> DDraw7IntfProxied = static_cast<IDirectDraw7*>(lplpDDProxied);
      *lplpDD = ref(new DDraw7Interface(nullptr, std::move(DDraw7IntfProxied)));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return S_OK;
  }

  HRESULT CreateDirectDraw(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter) {
    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDD);

    typedef HRESULT (__stdcall *DirectDrawCreate_t)(GUID *lpGUID, LPVOID *lplpDD, IUnknown *pUnkOuter);
    static DirectDrawCreate_t ProxiedDirectDrawCreate = nullptr;

    try {
      if (unlikely(ProxiedDirectDrawCreate == nullptr)) {
        HMODULE hDDraw = GetProxiedDDrawModule();

        if (unlikely(!hDDraw)) {
          Logger::err("CreateDirectDraw: Failed to load proxied ddraw.dll");
          return DDERR_GENERIC;
        }

        ProxiedDirectDrawCreate = reinterpret_cast<DirectDrawCreate_t>(::GetProcAddress(hDDraw, "DirectDrawCreate"));

        if (unlikely(ProxiedDirectDrawCreate == nullptr)) {
          Logger::err("CreateDirectDraw: Failed GetProcAddress");
          return DDERR_GENERIC;
        }
      }

      LPVOID lplpDDProxied = nullptr;
      HRESULT hr = ProxiedDirectDrawCreate(lpGUID, &lplpDDProxied, pUnkOuter);
      if (unlikely(FAILED(hr)))
        return hr;

      Com<IDirectDraw> DDrawIntfProxied = static_cast<IDirectDraw*>(lplpDDProxied);
      *lplpDD = ref(new DDrawInterface(nullptr, std::move(DDrawIntfProxied)));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return S_OK;
  }

  HRESULT CreateDirectDrawClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    typedef HRESULT (__stdcall *DirectDrawCreateClipper_t)(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter);
    static DirectDrawCreateClipper_t ProxiedDirectDrawCreateClipper = nullptr;

    if (unlikely(ProxiedDirectDrawCreateClipper == nullptr)) {
      HMODULE hDDraw = GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        Logger::err("DirectDrawCreateClipper: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDirectDrawCreateClipper = reinterpret_cast<DirectDrawCreateClipper_t>(GetProcAddress(hDDraw, "DirectDrawCreateClipper"));

      if (unlikely(ProxiedDirectDrawCreateClipper == nullptr)) {
        Logger::err("DirectDrawCreateClipper: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    Com<IDirectDrawClipper> lplpDDClipperProxy;
    HRESULT hr = ProxiedDirectDrawCreateClipper(dwFlags, &lplpDDClipperProxy, pUnkOuter);
    if (unlikely(FAILED(hr)))
      return hr;

    *lplpDDClipper = ref(new DDrawClipper(nullptr, std::move(lplpDDClipperProxy), nullptr));

    return S_OK;
  }

  HRESULT ClassFactoryCreateDirectDraw(IUnknown *pUnkOuter, REFIID riid, void **ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(pUnkOuter != nullptr))
      return CLASS_E_NOAGGREGATION;

    IDirectDraw* ppvObjectProxy = nullptr;
    HRESULT hr = CreateDirectDraw(NULL, &ppvObjectProxy, NULL);
    if (unlikely(FAILED(hr)))
      return hr;

    // ClassFactoryCreateDirectDraw can be used to construct objects
    // ranging from IDirectDraw to IDirectDraw2 and IDirectDraw4
    if (riid == __uuidof(IDirectDraw)) {
      *ppvObject = static_cast<void*>(ppvObjectProxy);
    } else if (riid == __uuidof(IDirectDraw2)) {
      void* directDraw2 = nullptr;
      hr = ppvObjectProxy->QueryInterface(__uuidof(IDirectDraw2), &directDraw2);
      if (unlikely(FAILED(hr))) {
        ppvObjectProxy->Release();
        return hr;
      }
      *ppvObject = directDraw2;
      ppvObjectProxy->Release();
    } else if (riid == __uuidof(IDirectDraw4)) {
      void* directDraw4 = nullptr;
      hr = ppvObjectProxy->QueryInterface(__uuidof(IDirectDraw4), &directDraw4);
      if (unlikely(FAILED(hr))) {
        ppvObjectProxy->Release();
        return hr;
      }
      *ppvObject = directDraw4;
      ppvObjectProxy->Release();
    // Apparently this also works, but I doubt is ever used in practice,
    // since IDirectDraw7 can be requested directly in DllGetClassObject
    } else if (riid == __uuidof(IDirectDraw7)) {
      ppvObjectProxy->Release();
      return CreateDirectDrawEx(NULL, ppvObject, riid, NULL);
    } else {
      Logger::warn(str::format(">>> ClassFactoryCreateDirectDraw: Unknown IID: ", riid));
      return CLASS_E_CLASSNOTAVAILABLE;
    }

    return S_OK;
  }

  HRESULT ClassFactoryCreateDirectDrawEx(IUnknown *pUnkOuter, REFIID riid, void **ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(pUnkOuter != nullptr))
      return CLASS_E_NOAGGREGATION;

    if (unlikely(riid != __uuidof(IDirectDraw7))) {
      Logger::warn(str::format(">>> ClassFactoryCreateDirectDrawEx: Unknown IID: ", riid));
      return CLASS_E_CLASSNOTAVAILABLE;
    }

    return CreateDirectDrawEx(NULL, ppvObject, riid, NULL);
  }

  HRESULT ClassFactoryCreateDirectDrawClipper(IUnknown *pUnkOuter, REFIID riid, void **ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(pUnkOuter != nullptr))
      return CLASS_E_NOAGGREGATION;

    if (unlikely(riid != __uuidof(IDirectDrawClipper)))
      return CLASS_E_CLASSNOTAVAILABLE;

    return CreateDirectDrawClipper(0, reinterpret_cast<IDirectDrawClipper**>(ppvObject), NULL);
  }

}

extern "C" {

  DLLEXPORT HRESULT __stdcall AcquireDDThreadLock() {
    typedef HRESULT (__stdcall *AcquireDDThreadLock_t)();
    static AcquireDDThreadLock_t ProxiedAcquireDDThreadLock = nullptr;

    if (unlikely(ProxiedAcquireDDThreadLock == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("AcquireDDThreadLock: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedAcquireDDThreadLock = reinterpret_cast<AcquireDDThreadLock_t>(GetProcAddress(hDDraw, "AcquireDDThreadLock"));

      if (unlikely(ProxiedAcquireDDThreadLock == nullptr)) {
        dxvk::Logger::err("AcquireDDThreadLock: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    return ProxiedAcquireDDThreadLock();
  }

  DLLEXPORT HRESULT __stdcall CompleteCreateSysmemSurface(DWORD arg) {
    dxvk::Logger::warn("!!! CompleteCreateSysmemSurface: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall D3DParseUnknownCommand(LPVOID lpCmd, LPVOID *lpRetCmd) {
    dxvk::Logger::warn("!!! D3DParseUnknownCommand: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall DDGetAttachedSurfaceLcl(DWORD arg1, DWORD arg2, DWORD arg3) {
    dxvk::Logger::warn("!!! DDGetAttachedSurfaceLcl: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall DDInternalLock(DWORD arg1, DWORD arg2) {
    dxvk::Logger::warn("!!! DDInternalLock: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall DDInternalUnlock(DWORD arg) {
    dxvk::Logger::warn("!!! DDInternalUnlock: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall DSoundHelp(DWORD arg1, DWORD arg2, DWORD arg3) {
    dxvk::Logger::warn("!!! DSoundHelp: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall DirectDrawCreate(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter) {
    return dxvk::CreateDirectDraw(lpGUID, lplpDD, pUnkOuter);
  }

  // Mostly unused, except for Sea Dogs (D3D6)
  DLLEXPORT HRESULT __stdcall DirectDrawCreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    return dxvk::CreateDirectDrawClipper(dwFlags, lplpDDClipper, pUnkOuter);
  }

  DLLEXPORT HRESULT __stdcall DirectDrawCreateEx(GUID *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown *pUnkOuter) {
    return dxvk::CreateDirectDrawEx(lpGUID, lplpDD, iid, pUnkOuter);
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateA(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext) {
    typedef HRESULT (__stdcall *DirectDrawEnumerateA_t)(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext);
    static DirectDrawEnumerateA_t ProxiedDirectDrawEnumerateA = nullptr;

    if (unlikely(ProxiedDirectDrawEnumerateA == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateA: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDirectDrawEnumerateA = reinterpret_cast<DirectDrawEnumerateA_t>(GetProcAddress(hDDraw, "DirectDrawEnumerateA"));

      if (unlikely(ProxiedDirectDrawEnumerateA == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateA: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    return ProxiedDirectDrawEnumerateA(lpCallback, lpContext);
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateExA(LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags) {
    typedef HRESULT (__stdcall *DirectDrawEnumerateExA_t)(LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags);
    static DirectDrawEnumerateExA_t ProxiedDirectDrawEnumerateExA = nullptr;

    if (unlikely(ProxiedDirectDrawEnumerateExA == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateExA: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDirectDrawEnumerateExA = reinterpret_cast<DirectDrawEnumerateExA_t>(GetProcAddress(hDDraw, "DirectDrawEnumerateExA"));

      if (unlikely(ProxiedDirectDrawEnumerateExA == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateExA: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    return ProxiedDirectDrawEnumerateExA(lpCallback, lpContext, dwFlags);
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateExW(LPDDENUMCALLBACKEXW lpCallback, LPVOID lpContext, DWORD dwFlags) {
    typedef HRESULT (__stdcall *DirectDrawEnumerateExW_t)(LPDDENUMCALLBACKEXW lpCallback, LPVOID lpContext, DWORD dwFlags);
    static DirectDrawEnumerateExW_t ProxiedDirectDrawEnumerateExW = nullptr;

    if (unlikely(ProxiedDirectDrawEnumerateExW == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateExW: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDirectDrawEnumerateExW = reinterpret_cast<DirectDrawEnumerateExW_t>(GetProcAddress(hDDraw, "DirectDrawEnumerateExW"));

      if (unlikely(ProxiedDirectDrawEnumerateExW == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateExW: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    return ProxiedDirectDrawEnumerateExW(lpCallback, lpContext, dwFlags);
  }

  DLLEXPORT HRESULT __stdcall DirectDrawEnumerateW(LPDDENUMCALLBACKW lpCallback, LPVOID lpContext) {
    typedef HRESULT (__stdcall *DirectDrawEnumerateW_t)(LPDDENUMCALLBACKW lpCallback, LPVOID lpContext);
    static DirectDrawEnumerateW_t ProxiedDirectDrawEnumerateW = nullptr;

    if (unlikely(ProxiedDirectDrawEnumerateW == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateW: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDirectDrawEnumerateW = reinterpret_cast<DirectDrawEnumerateW_t>(GetProcAddress(hDDraw, "DirectDrawEnumerateW"));

      if (unlikely(ProxiedDirectDrawEnumerateW == nullptr)) {
        dxvk::Logger::err("DirectDrawEnumerateW: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    return ProxiedDirectDrawEnumerateW(lpCallback, lpContext);
  }

  DLLEXPORT HRESULT __stdcall DllCanUnloadNow() {
    typedef HRESULT (__stdcall *DllCanUnloadNow_t)();
    static DllCanUnloadNow_t ProxiedDllCanUnloadNow = nullptr;

    if (unlikely(ProxiedDllCanUnloadNow == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("DllCanUnloadNow: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedDllCanUnloadNow = reinterpret_cast<DllCanUnloadNow_t>(GetProcAddress(hDDraw, "DllCanUnloadNow"));

      if (unlikely(ProxiedDllCanUnloadNow == nullptr)) {
        dxvk::Logger::err("DllCanUnloadNow: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    return ProxiedDllCanUnloadNow();
  }

  DLLEXPORT HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
    if (unlikely(ppv == nullptr))
      return E_POINTER;

    //dxvk::Logger::debug(dxvk::str::format("DllGetClassObject: Call for rclsid: ", rclsid));

    if (unlikely(riid != __uuidof(IClassFactory) && riid != __uuidof(IUnknown)))
      return E_NOINTERFACE;

    if (likely(rclsid == dxvk::CLSID_DirectDraw)) {
      dxvk::DDrawClassFactory* ddrawClassFactory = new dxvk::DDrawClassFactory(dxvk::ClassFactoryCreateDirectDraw);

      *ppv = ref(ddrawClassFactory);

      return S_OK;
    } else if (rclsid == dxvk::CLSID_DirectDraw7) {
      dxvk::DDrawClassFactory* ddrawClassFactory = new dxvk::DDrawClassFactory(dxvk::ClassFactoryCreateDirectDrawEx);

      *ppv = ref(ddrawClassFactory);

      return S_OK;
    } else if (rclsid == dxvk::CLSID_DirectDrawClipper) {
      dxvk::DDrawClassFactory* ddrawClassFactory = new dxvk::DDrawClassFactory(dxvk::ClassFactoryCreateDirectDrawClipper);

      *ppv = ref(ddrawClassFactory);

      return S_OK;
    }

    return CLASS_E_CLASSNOTAVAILABLE;
  }

  DLLEXPORT HRESULT __stdcall GetDDSurfaceLocal(DWORD arg1, DWORD arg2, DWORD arg3) {
    dxvk::Logger::warn("!!! GetDDSurfaceLocal: Stub");
    return S_OK;
  }

  DLLEXPORT HANDLE __stdcall GetOLEThunkData(DWORD index) {
    dxvk::Logger::warn("!!! GetOLEThunkData: Stub");
    return 0;
  }

  DLLEXPORT HRESULT __stdcall GetSurfaceFromDC(HDC hdc, LPDIRECTDRAWSURFACE7 *lpDDS, DWORD arg) {
    dxvk::Logger::warn("!!! GetSurfaceFromDC: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall InternalLock(DWORD arg1, DWORD arg2) {
    dxvk::Logger::warn("!!! InternalLock: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall InternalUnlock(DWORD arg) {
    dxvk::Logger::warn("!!! InternalUnlock: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall RegisterSpecialCase(DWORD arg1, DWORD arg2, DWORD arg3, DWORD arg4) {
    dxvk::Logger::warn("!!! RegisterSpecialCase: Stub");
    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall ReleaseDDThreadLock() {
    typedef HRESULT (__stdcall *ReleaseDDThreadLock_t)();
    static ReleaseDDThreadLock_t ProxiedReleaseDDThreadLock = nullptr;

    if (unlikely(ProxiedReleaseDDThreadLock == nullptr)) {
      HMODULE hDDraw = dxvk::GetProxiedDDrawModule();

      if (unlikely(hDDraw == nullptr)) {
        dxvk::Logger::err("ReleaseDDThreadLock: Failed to load proxied ddraw.dll");
        return DDERR_GENERIC;
      }

      ProxiedReleaseDDThreadLock = reinterpret_cast<ReleaseDDThreadLock_t>(GetProcAddress(hDDraw, "ReleaseDDThreadLock"));

      if (unlikely(ProxiedReleaseDDThreadLock == nullptr)) {
        dxvk::Logger::err("ReleaseDDThreadLock: Failed GetProcAddress");
        return DDERR_GENERIC;
      }
    }

    return ProxiedReleaseDDThreadLock();
  }

  BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
      case DLL_THREAD_ATTACH:
        break;
      case DLL_THREAD_DETACH:
        break;
      case DLL_PROCESS_ATTACH:
        dxvk::Logger::info(">>>>>>> LOADING D7VK >>>>>>>");
        break;
      case DLL_PROCESS_DETACH: {
        // Calling FreeLibrary on DLL_PROCESS_DETACH is technically discouraged,
        // however apitrace appears to do it with no ill effect, and I have no
        // other ideas on how to properly free up the proxied ddraw.dll.
        HMODULE hDDraw = dxvk::GetProxiedDDrawModule();
        if (likely(hDDraw != nullptr))
          FreeLibrary(hDDraw);
        dxvk::Logger::info("<<<<<<< UNLOADING D7VK <<<<<<<");
        break;
      }
      default:
        break;
    }
    return TRUE;
  }

}
