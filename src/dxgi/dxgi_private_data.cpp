#include <cmath>
#include <cstring>
#include <cstdlib>

#include "dxgi_private_data.h"

namespace dxvk {
  
  DxgiPrivateDataEntry::DxgiPrivateDataEntry() { }
  DxgiPrivateDataEntry::DxgiPrivateDataEntry(
          REFGUID   guid,
          UINT      size,
    const void*     data)
  : m_guid(guid),
    m_size(size),
    m_data(std::malloc(size)) {
    std::memcpy(m_data, data, size);
  }
  
  
  DxgiPrivateDataEntry::DxgiPrivateDataEntry(
          REFGUID   guid,
    const IUnknown* iface)
  : m_guid  (guid),
    m_iface (const_cast<IUnknown*>(iface)) {
    m_iface->AddRef();
  }
  
  
  DxgiPrivateDataEntry::~DxgiPrivateDataEntry() {
    this->destroy();
  }
  
  
  DxgiPrivateDataEntry::DxgiPrivateDataEntry(DxgiPrivateDataEntry&& other)
  : m_guid  (other.m_guid),
    m_size  (other.m_size),
    m_data  (other.m_data),
    m_iface (other.m_iface) {
    other.m_guid  = __uuidof(IUnknown);
    other.m_size  = 0;
    other.m_data  = nullptr;
    other.m_iface = nullptr;
  }
  
  
  DxgiPrivateDataEntry& DxgiPrivateDataEntry::operator = (DxgiPrivateDataEntry&& other) {
    this->destroy();
    this->m_guid  = other.m_guid;
    this->m_size  = other.m_size;
    this->m_data  = other.m_data;
    this->m_iface = other.m_iface;
    
    other.m_guid  = __uuidof(IUnknown);
    other.m_size  = 0;
    other.m_data  = nullptr;
    other.m_iface = nullptr;
    return *this;
  }
  
  
  HRESULT DxgiPrivateDataEntry::get(UINT& size, void* data) const {
    if (size != 0 && data == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    const UINT minSize = m_iface != nullptr
      ? sizeof(IUnknown*)
      : m_size;
    
    const HRESULT result = size < minSize
      ? DXGI_ERROR_MORE_DATA
      : S_OK;
    
    if (size >= minSize) {
      if (m_iface != nullptr) {
        m_iface->AddRef();
        std::memcpy(data, &m_iface, minSize);
      } else {
        std::memcpy(data, m_data, minSize);
      }
    }
    
    size = minSize;
    return result;
  }
  
  
  void DxgiPrivateDataEntry::destroy() {
    if (m_data != nullptr)
      std::free(m_data);
    if (m_iface != nullptr)
      m_iface->Release();
  }
  
  
  HRESULT DxgiPrivateData::setData(
          REFGUID   guid,
          UINT      size,
    const void*     data) {
    this->insertEntry(DxgiPrivateDataEntry(guid, size, data));
    return S_OK;
  }
  
  
  HRESULT DxgiPrivateData::setInterface(
          REFGUID   guid,
    const IUnknown* iface) {
    this->insertEntry(DxgiPrivateDataEntry(guid, iface));
    return S_OK;
  }
  
  
  HRESULT DxgiPrivateData::getData(
          REFGUID   guid,
          UINT*     size,
          void*     data) {
    if (size == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    auto entry = this->findEntry(guid);
    
    if (entry == nullptr)
      return DXGI_ERROR_NOT_FOUND;
    
    return entry->get(*size, data);
  }
  
  
  DxgiPrivateDataEntry* DxgiPrivateData::findEntry(REFGUID guid) {
    for (DxgiPrivateDataEntry& e : m_entries) {
      if (e.hasGuid(guid))
        return &e;
    }
    
    return nullptr;
  }
  
  
  void DxgiPrivateData::insertEntry(DxgiPrivateDataEntry&& entry) {
    DxgiPrivateDataEntry  srcEntry = std::move(entry);
    DxgiPrivateDataEntry* dstEntry = this->findEntry(srcEntry.guid());
    
    if (dstEntry != nullptr)
      *dstEntry = std::move(srcEntry);
    else
      m_entries.push_back(std::move(srcEntry));
  }
  
}
