#include "d3d9_device.h"

#include <d3dx9math.h>

#define CHECK_STATE(state) { if (!CheckState((state))) { return D3DERR_INVALIDCALL; } }

namespace dxvk {
  /// Checks whether a given matrix index is valid.
  static bool CheckState(D3DTRANSFORMSTATETYPE State) {
    if (State <= 23)
      return true;
    if (256 <= State && State <= 512)
      return true;
    return false;
  }

  // Declare the identity matrix here since it's used a few times.
  static constexpr D3DMATRIX M_IDENTITY {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
  };

  HRESULT D3D9Device::GetTransform(D3DTRANSFORMSTATETYPE State,
    D3DMATRIX* pMatrix) {
    CHECK_STATE(State);
    CHECK_NOT_NULL(pMatrix);

    const auto tr = m_transforms.find(State);

    if (tr != m_transforms.cend()) {
      *pMatrix = tr->second;
    } else {
      // Return the null matrix in any other case to be safe.
      *pMatrix = {};
    }

    return D3D_OK;
  }

  HRESULT D3D9Device::SetTransform(D3DTRANSFORMSTATETYPE State,
    const D3DMATRIX* pMatrix) {
    CHECK_STATE(State);
    CHECK_NOT_NULL(pMatrix);

    m_transforms.insert_or_assign(State, *pMatrix);

    return D3D_OK;
  }

  // This function composes an existing transform matrix with another one.
  HRESULT D3D9Device::MultiplyTransform(D3DTRANSFORMSTATETYPE State,
    const D3DMATRIX* pMatrix) {
    CHECK_STATE(State);
    CHECK_NOT_NULL(pMatrix);

    const auto tr = m_transforms.find(State);

    if (tr == m_transforms.end())
      return D3DERR_INVALIDCALL;

    // Even though it's another name, the matrix layouts are the same.
    const auto m = reinterpret_cast<D3DXMATRIX*>(&tr->second);
    const auto n = reinterpret_cast<const D3DXMATRIX*>(pMatrix);

    // Not sure if we could do the multiplication in place, better use a buffer.
    D3DXMATRIX buf;

    D3DXMatrixMultiply(&buf, m, n);

    *m = buf;

    return D3D_OK;
  }
}
