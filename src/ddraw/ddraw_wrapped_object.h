#pragma once

#include "ddraw_include.h"
#include "ddraw_child_object.h"

namespace dxvk {

  template <typename ParentType, typename DDrawType>
  class DDrawWrappedObject : public DDrawChildObject<ParentType, DDrawType> {

  public:

    using Parent = ParentType;
    using DDraw = DDrawType;

    DDrawWrappedObject(Parent* parent, Com<DDraw>&& proxiedIntf)
      : DDrawChildObject<ParentType, DDrawType>(parent)
      , m_proxy ( std::move(proxiedIntf) ) {
    }

    DDraw* GetProxied() const {
      return m_proxy.ptr();
    }

  protected:

    Com<DDraw> m_proxy;

  };

}