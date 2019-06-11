#include <cmath>
#include <cstring>
#include <cstdlib>

#include "com_private_data.h"

namespace dxvk {
  
  ComPrivateDataEntry::ComPrivateDataEntry() { }
  ComPrivateDataEntry::ComPrivateDataEntry(
          REFGUID   guid,
          UINT      size,
    const void*     data)
  : m_guid(guid),
    m_type(ComPrivateDataType::Data),
    m_size(size),
    m_data(std::malloc(size)) {
    std::memcpy(m_data, data, size);
  }
  
  
  ComPrivateDataEntry::ComPrivateDataEntry(
          REFGUID   guid,
    const IUnknown* iface)
  : m_guid  (guid),
    m_type(ComPrivateDataType::Iface),
    m_iface (const_cast<IUnknown*>(iface)) {
    if (m_iface)
      m_iface->AddRef();
  }
  
  
  ComPrivateDataEntry::~ComPrivateDataEntry() {
    this->destroy();
  }
  
  
  ComPrivateDataEntry::ComPrivateDataEntry(ComPrivateDataEntry&& other)
  : m_guid  (other.m_guid),
    m_type  (other.m_type),
    m_size  (other.m_size),
    m_data  (other.m_data),
    m_iface (other.m_iface) {
    other.m_guid  = __uuidof(IUnknown);
    other.m_type  = ComPrivateDataType::None;
    other.m_size  = 0;
    other.m_data  = nullptr;
    other.m_iface = nullptr;
  }
  
  
  ComPrivateDataEntry& ComPrivateDataEntry::operator = (ComPrivateDataEntry&& other) {
    this->destroy();
    this->m_guid  = other.m_guid;
    this->m_type  = other.m_type;
    this->m_size  = other.m_size;
    this->m_data  = other.m_data;
    this->m_iface = other.m_iface;
    
    other.m_guid  = __uuidof(IUnknown);
    other.m_type  = ComPrivateDataType::None;
    other.m_size  = 0;
    other.m_data  = nullptr;
    other.m_iface = nullptr;
    return *this;
  }
  
  
  HRESULT ComPrivateDataEntry::get(UINT& size, void* data) const {
    UINT minSize = 0;

    if (m_type == ComPrivateDataType::Iface) minSize = sizeof(IUnknown*);
    if (m_type == ComPrivateDataType::Data)  minSize = m_size;
    
    if (!data) {
      size = minSize;
      return S_OK;
    }

    HRESULT result = size < minSize
      ? DXGI_ERROR_MORE_DATA
      : S_OK;
    
    if (size >= minSize) {
      if (m_type == ComPrivateDataType::Iface) {
        if (m_iface)
          m_iface->AddRef();
        std::memcpy(data, &m_iface, minSize);
      } else {
        std::memcpy(data, m_data, minSize);
      }
    }
    
    size = minSize;
    return result;
  }
  
  
  void ComPrivateDataEntry::destroy() {
    if (m_data)
      std::free(m_data);
    if (m_iface)
      m_iface->Release();
  }
  
  
  HRESULT ComPrivateData::setData(
          REFGUID   guid,
          UINT      size,
    const void*     data) {
    if (!data) {
      for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (it->hasGuid(guid)) {
          m_entries.erase(it);
          return S_OK;
        }
      }
      return S_FALSE;
    }
    this->insertEntry(ComPrivateDataEntry(guid, size, data));
    return S_OK;
  }
  
  
  HRESULT ComPrivateData::setInterface(
          REFGUID   guid,
    const IUnknown* iface) {
    this->insertEntry(ComPrivateDataEntry(guid, iface));
    return S_OK;
  }
  
  
  HRESULT ComPrivateData::getData(
          REFGUID   guid,
          UINT*     size,
          void*     data) {
    if (!size)
      return E_INVALIDARG;
    
    auto entry = this->findEntry(guid);
    
    if (!entry) {
      *size = 0;
      return DXGI_ERROR_NOT_FOUND;
    }
    
    return entry->get(*size, data);
  }
  
  
  ComPrivateDataEntry* ComPrivateData::findEntry(REFGUID guid) {
    for (ComPrivateDataEntry& e : m_entries) {
      if (e.hasGuid(guid))
        return &e;
    }
    
    return nullptr;
  }
  
  
  void ComPrivateData::insertEntry(ComPrivateDataEntry&& entry) {
    ComPrivateDataEntry  srcEntry = std::move(entry);
    ComPrivateDataEntry* dstEntry = this->findEntry(srcEntry.guid());
    
    if (dstEntry)
      *dstEntry = std::move(srcEntry);
    else
      m_entries.push_back(std::move(srcEntry));
  }
  
}
