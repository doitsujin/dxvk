#pragma once

namespace dxvk {

  class D3D9Cursor {

  public:

    D3D9Cursor();

    void UpdateCursor(int x, int y, bool immediate);

    void FlushCursor();

  private:

    bool m_updatePending;
    int m_pendingX;
    int m_pendingY;

  };

}