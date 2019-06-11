#pragma once

#include <vector>

#include "com_include.h"

namespace dxvk {
  
  /**
   * \brief COM private data entry type
   */
  enum ComPrivateDataType {
    None,
    Data,
    Iface,
  };
  
  /**
   * \brief Data entry for private storage
   * Stores a single private storage item.
   */
  class ComPrivateDataEntry {
    
  public:
    
    ComPrivateDataEntry();
    ComPrivateDataEntry(
            REFGUID   guid,
            UINT      size,
      const void*     data);
    ComPrivateDataEntry(
            REFGUID   guid,
      const IUnknown* iface);
    ~ComPrivateDataEntry();
    
    ComPrivateDataEntry             (ComPrivateDataEntry&& other);
    ComPrivateDataEntry& operator = (ComPrivateDataEntry&& other);
    
    /**
     * \brief The entry's GUID
     * \returns The GUID
     */
    REFGUID guid() const {
      return m_guid;
    }
    
    /**
     * \brief Checks whether the GUID matches another one
     * 
     * GUIDs are used to identify private data entries.
     * \param [in] guid The GUID to compare to
     * \returns \c true if this entry holds the same GUID
     */
    bool hasGuid(REFGUID guid) const {
      return m_guid == guid;
    }
    
    /**
     * \brief Retrieves stored data
     * 
     * \param [in,out] size Destination buffer size
     * \param [in] data Appliaction-provided buffer
     * \returns \c S_OK on success, or \c DXGI_ERROR_MORE_DATA
     *          if the destination buffer is too small
     */
    HRESULT get(UINT& size, void* data) const;
    
  private:
    
    GUID                m_guid  = __uuidof(IUnknown);
    ComPrivateDataType  m_type  = ComPrivateDataType::None;
    UINT                m_size  = 0;
    void*               m_data  = nullptr;
    IUnknown*           m_iface = nullptr;
    
    void destroy();
    
  };
  
  
  /**
   * \brief Private storage for DXGI objects
   * 
   * Provides storage for application-defined
   * byte arrays or COM interfaces that can be
   * retrieved using GUIDs.
   */
  class ComPrivateData {
    
  public:
    
    HRESULT setData(
            REFGUID   guid,
            UINT      size,
      const void*     data);
    
    HRESULT setInterface(
            REFGUID   guid,
      const IUnknown* iface);
    
    HRESULT getData(
            REFGUID   guid,
            UINT*     size,
            void*     data);
    
  private:
    
    std::vector<ComPrivateDataEntry> m_entries;
    
    ComPrivateDataEntry* findEntry(REFGUID guid);
    void insertEntry(ComPrivateDataEntry&& entry);
    
  };
  
}
