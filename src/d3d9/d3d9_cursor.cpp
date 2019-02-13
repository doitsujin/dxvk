#include "d3d9_cursor.h"

#include <windows.h>

namespace dxvk {

  D3D9Cursor::D3D9Cursor()
    : m_updatePending{ false }
    , m_pendingX{ 0 }
    , m_pendingY{ 0 } {}

  void D3D9Cursor::updateCursor(int x, int y, bool immediate) {
    m_updatePending = true;
    m_pendingX = x;
    m_pendingY = y;

    if (immediate)
      flushCursor();
  }

  void D3D9Cursor::flushCursor() {
    ::SetCursorPos(m_pendingX, m_pendingY);

    m_updatePending = false;
  }

}