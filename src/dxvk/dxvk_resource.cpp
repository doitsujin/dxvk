#include "dxvk_resource.h"

namespace dxvk {
  
  std::atomic<uint64_t> DxvkResource::s_cookie = { 0ull };


  DxvkResource::DxvkResource()
  : m_useCount(0ull), m_cookie(++s_cookie) {

  }


  DxvkResource::~DxvkResource() {
    
  }
  
}