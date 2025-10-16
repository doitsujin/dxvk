#pragma once

#include "ddraw_include.h"

namespace dxvk {

  template <typename ParentType, typename DDrawType>
  class DDrawChildObject : public ComObjectClamp<DDrawType> {

  public:

    using Parent = ParentType;

    DDrawChildObject(Parent* parent)
      : m_parent ( parent ) {
    }

    void UpdateParent(Parent* parent) {
      m_parent = parent;
    }

    Parent* GetParent() const {
      return m_parent;
    }

  protected:

    Parent* m_parent = nullptr;

  };

}