#pragma once

#include "../dxgi/dxgi_include.h"
#include "../util/sync/sync_spinlock.h"

#include <d3d10_1.h>
#include <d3d11_1.h>

#ifdef __WINE__
extern "C++" {
  template<typename T> void **IID_PPV_ARGS_Helper (T **pp) {
    static_cast<IUnknown *> (*pp);
    return reinterpret_cast<void **> (pp);
  }
}

#define IID_PPV_ARGS(ppType) __uuidof (**(ppType)), IID_PPV_ARGS_Helper (ppType)
#endif // __WINE__
