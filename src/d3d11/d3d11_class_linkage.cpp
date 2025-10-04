#include "d3d11_class_linkage.h"
#include "d3d11_device.h"

namespace dxvk {

  D3D11ClassInstance::D3D11ClassInstance(
          D3D11Device*                pDevice,
          D3D11ClassLinkage*          pLinkage,
    const D3D11_CLASS_INSTANCE_DESC*  pDesc,
    const char*                       pInstanceName,
    const char*                       pTypeName,
    const D3D11ClassTypeInfo*         pTypeInfo)
  : D3D11DeviceObject<ID3D11ClassInstance>(pDevice),
    m_linkage             (pLinkage),
    m_destructionNotifier (this),
    m_desc                (*pDesc) {
    if (pInstanceName)
      m_instanceName = pInstanceName;

    if (pTypeName)
      m_typeName = pTypeName;

    if (pTypeInfo)
      m_type = *pTypeInfo;
  }


  D3D11ClassInstance::~D3D11ClassInstance() {

  }


  ULONG STDMETHODCALLTYPE D3D11ClassInstance::AddRef() {
    auto newCount = ++m_refCount;

    if (newCount == 1u)
      AddRefPrivate();

    return newCount;
  }


  ULONG STDMETHODCALLTYPE D3D11ClassInstance::Release() {
    auto newCount = --m_refCount;

    if (newCount == 0u)
      ReleasePrivate();

    return newCount;
  }


  void D3D11ClassInstance::AddRefPrivate() {
    if (!(m_refPrivate++))
      m_linkage->AddRefPrivate();
  }


  void D3D11ClassInstance::ReleasePrivate() {
    if (!(--m_refPrivate))
      m_linkage->ReleasePrivate();
  }


  HRESULT STDMETHODCALLTYPE D3D11ClassInstance::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11ClassInstance)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3DDestructionNotifier)) {
      *ppvObject = ref(&m_destructionNotifier);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11ClassInstance), riid)) {
      Logger::warn("D3D11ClassLinkage::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  void STDMETHODCALLTYPE D3D11ClassInstance::GetClassLinkage(
          ID3D11ClassLinkage**    ppLinkage) {
    *ppLinkage = ref(m_linkage);
  }


  void STDMETHODCALLTYPE D3D11ClassInstance::GetDesc(
          D3D11_CLASS_INSTANCE_DESC* pDesc) {
    *pDesc = m_desc;
  }


  void STDMETHODCALLTYPE D3D11ClassInstance::GetInstanceName(
          LPSTR                   pInstanceName,
          SIZE_T*                 pBufferLength) {
    ReturnName(pInstanceName, pBufferLength,
      m_desc.Created ? std::string() : m_instanceName);
  }


  void STDMETHODCALLTYPE D3D11ClassInstance::GetTypeName(
          LPSTR                   pTypeName,
          SIZE_T*                 pBufferLength) {
    ReturnName(pTypeName, pBufferLength,
      m_desc.Created ? m_typeName : std::string());
  }


  bool D3D11ClassInstance::MatchesTypeName(
    const char*                   pName) const {
    return !std::strncmp(pName, m_typeName.c_str(), m_typeName.size());
  }


  void D3D11ClassInstance::ReturnName(
          LPSTR                   pName,
          SIZE_T*                 pLength,
    const std::string&            SrcName) {
    if (pName)
      str::strlcpy(pName, SrcName.c_str(), *pLength);

    // Include null-terminator
    *pLength = SrcName.size() + 1u;
  }




  D3D11ClassLinkage::D3D11ClassLinkage(
          D3D11Device*                pDevice)
  : D3D11DeviceChild<ID3D11ClassLinkage>(pDevice),
    m_destructionNotifier(this) {
    
  }
  
  
  D3D11ClassLinkage::~D3D11ClassLinkage() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ClassLinkage::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11ClassLinkage)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3DDestructionNotifier)) {
      *ppvObject = ref(&m_destructionNotifier);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11ClassLinkage), riid)) {
      Logger::warn("D3D11ClassLinkage::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ClassLinkage::CreateClassInstance(
          LPCSTR              pClassTypeName,
          UINT                ConstantBufferOffset,
          UINT                ConstantVectorOffset,
          UINT                TextureOffset,
          UINT                SamplerOffset,
          ID3D11ClassInstance **ppInstance) {
    InitReturnPtr(ppInstance);

    if (!ppInstance)
      return S_FALSE;

    // There is no deduplication or persistent storage for these going on
    D3D11_CLASS_INSTANCE_DESC desc = { };
    desc.TypeId = AddType(pClassTypeName, 0u, 0u, 0u).typeId;
    desc.ConstantBuffer = ConstantBufferOffset;
    desc.BaseConstantBufferOffset = ConstantVectorOffset;
    desc.BaseTexture = TextureOffset;
    desc.BaseSampler = SamplerOffset;
    desc.Created = true;

    *ppInstance = ref(new D3D11ClassInstance(m_parent,
      this, &desc, nullptr, pClassTypeName, nullptr));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ClassLinkage::GetClassInstance(
          LPCSTR              pClassInstanceName,
          UINT                InstanceIndex,
          ID3D11ClassInstance **ppInstance) {
    InitReturnPtr(ppInstance);

    std::lock_guard lock(m_mutex);

    // It is possible to "get" an instance before any shader containing
    // an instance with the name is created. The resulting instance will
    // have a default set of parameters.
    D3D11ClassTypeInfo typeInfo = { };

    InstanceInfo info = { };
    info.desc.InstanceId = m_instances.size();
    info.desc.InstanceIndex = InstanceIndex;

    auto result = m_instances.emplace(std::piecewise_construct,
      std::tuple(pClassInstanceName),
      std::tuple(info));

    if (!result.second) {
      info = result.first->second;

      typeInfo.CbvStride = info.typeInfo.cbvStride;
      typeInfo.SrvCount = info.typeInfo.srvCount;
      typeInfo.SamplerCount = info.typeInfo.samplerCount;
    }

    // Once again, no persistent storage here at all, the runtime
    // will return different objects even if the parameters match.
    *ppInstance = ref(new D3D11ClassInstance(m_parent,
      this, &info.desc, pClassInstanceName, info.typeName.c_str(), &typeInfo));
    return S_OK;
  }


  D3D11ClassLinkage::TypeInfo D3D11ClassLinkage::AddType(
          LPCSTR              pTypeName,
          UINT                CbvStride,
          UINT                SrvCount,
          UINT                SamplerCount) {
    std::lock_guard lock(m_mutex);

    TypeInfo result = { };
    result.typeId = m_types.size();
    result.cbvStride = CbvStride;
    result.srvCount = SrvCount;
    result.samplerCount = SamplerCount;

    auto entry = m_types.emplace(std::piecewise_construct,
      std::tuple(pTypeName),
      std::tuple(result));

    // Return existing type info if any
    return entry.first->second;
  }


  void D3D11ClassLinkage::AddInstance(
    const D3D11_CLASS_INSTANCE_DESC* pDesc,
          LPCSTR              pTypeName,
          LPCSTR              pInstanceName) {
    std::lock_guard lock(m_mutex);

    InstanceInfo info = { };
    info.typeName = pTypeName;
    info.desc = *pDesc;
    info.desc.InstanceId = m_instances.size();

    auto type = m_types.find(pTypeName);

    if (type != m_types.end()) {
      info.typeInfo = type->second;
      info.desc.TypeId = info.typeInfo.typeId;
    }

    m_instances.emplace(std::piecewise_construct,
      std::tuple(pInstanceName),
      std::tuple(info));
  }

}
