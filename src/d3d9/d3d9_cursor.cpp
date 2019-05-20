#include "d3d9_cursor.h"

#include <utility>

namespace dxvk {

  D3D9Cursor::D3D9Cursor()
    : m_updatePending( false )
    , m_pendingX     ( 0 )
    , m_pendingY     ( 0 ) {}

  void D3D9Cursor::UpdateCursor(int x, int y, bool immediate) {
    m_updatePending = true;
    m_pendingX = x;
    m_pendingY = y;

    if (immediate)
      FlushCursor();
  }

  void D3D9Cursor::FlushCursor() {
    ::SetCursorPos(m_pendingX, m_pendingY);

    m_updatePending = false;
  }

  BOOL D3D9Cursor::ShowCursor(BOOL bShow) {
    return std::exchange(m_updatePending, bShow);
  }

}