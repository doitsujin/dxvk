#pragma once

namespace dxvk {

  class D3D9Cursor {

  public:

    D3D9Cursor();

    void updateCursor(int x, int y, bool immediate);

    void flushCursor();

  private:

    bool m_updatePending;
    int m_pendingX;
    int m_pendingY;

  };

}