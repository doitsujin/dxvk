#pragma once

#include <mutex>
#include <unordered_map>

#include "../dxvk/dxvk_hash.h"

#include "../util/thread.h"

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Device;
  class D3D11ClassLinkage;

  struct D3D11ClassTypeInfo {
    uint32_t CbvStride = 0u;
    uint32_t SrvCount = 0u;
    uint32_t SamplerCount = 0u;
  };

  class D3D11ClassInstance : public D3D11DeviceObject<ID3D11ClassInstance> {

  public:

    D3D11ClassInstance(
            D3D11Device*                pDevice,
            D3D11ClassLinkage*          pLinkage,
      const D3D11_CLASS_INSTANCE_DESC*  pDesc,
      const char*                       pInstanceName,
      const char*                       pTypeName,
      const D3D11ClassTypeInfo*         pTypeInfo);

    ~D3D11ClassInstance();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    void STDMETHODCALLTYPE GetClassLinkage(
            ID3D11ClassLinkage**    ppLinkage);

    void STDMETHODCALLTYPE GetDesc(
            D3D11_CLASS_INSTANCE_DESC* pDesc);

    void STDMETHODCALLTYPE GetInstanceName(
            LPSTR                   pInstanceName,
            SIZE_T*                 pBufferLength);

    void STDMETHODCALLTYPE GetTypeName(
            LPSTR                   pTypeName,
            SIZE_T*                 pBufferLength);

    bool MatchesTypeName(
      const char*                   pName) const;

    void AddRefPrivate();

    void ReleasePrivate();

  private:

    std::atomic<uint32_t> m_refCount = { 0u };
    std::atomic<uint32_t> m_refPrivate = { 0u };

    D3D11ClassLinkage* m_linkage = nullptr;
    D3DDestructionNotifier m_destructionNotifier;

    D3D11_CLASS_INSTANCE_DESC m_desc = { };
    D3D11ClassTypeInfo m_type = { };

    std::string m_instanceName;
    std::string m_typeName;

    void ReturnName(
            LPSTR                   pName,
            SIZE_T*                 pLength,
      const std::string&            SrcName);

  };


  class D3D11ClassLinkage : public D3D11DeviceChild<ID3D11ClassLinkage> {
    struct TypeInfo {
      uint32_t typeId = 0u;
      uint32_t cbvStride = 0u;
      uint32_t srvCount = 0u;
      uint32_t samplerCount = 0u;
    };

    struct InstanceInfo {
      std::string typeName;
      TypeInfo typeInfo = { };
      D3D11_CLASS_INSTANCE_DESC desc = { };
    };
  public:
    
    D3D11ClassLinkage(
            D3D11Device*                pDevice);
    
    ~D3D11ClassLinkage();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE CreateClassInstance(
            LPCSTR              pTypeName,
            UINT                ConstantBufferOffset,
            UINT                ConstantVectorOffset,
            UINT                TextureOffset,
            UINT                SamplerOffset,
            ID3D11ClassInstance **ppInstance);
    
    HRESULT STDMETHODCALLTYPE GetClassInstance(
            LPCSTR              pInstanceName,
            UINT                InstanceIndex,
            ID3D11ClassInstance **ppInstance);

    TypeInfo AddType(
            LPCSTR              pTypeName,
            UINT                CbvStride,
            UINT                SrvCount,
            UINT                SamplerCount);

    void AddInstance(
      const D3D11_CLASS_INSTANCE_DESC* pDesc,
            LPCSTR              pTypeName,
            LPCSTR              pInstanceName);

  private:

    D3DDestructionNotifier m_destructionNotifier;

    dxvk::mutex m_mutex;

    std::unordered_map<std::string, InstanceInfo> m_instances;
    std::unordered_map<std::string, TypeInfo> m_types;

  };
  
}
