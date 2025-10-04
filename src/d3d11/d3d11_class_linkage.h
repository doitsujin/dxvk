#pragma once

#include <mutex>
#include <unordered_map>

#include "../dxvk/dxvk_hash.h"

#include "../util/thread.h"
#include "../util/util_small_vector.h"

#include "d3d11_device_child.h"

namespace dxvk {

  class D3D11Device;
  class D3D11ClassLinkage;
  class D3D11ClassInstance;

  struct D3D11ClassTypeInfo {
    uint32_t CbvStride = 0u;
    uint32_t SrvCount = 0u;
    uint32_t SamplerCount = 0u;
  };


  struct D3D11InterfaceType {
    uint32_t typeId = 0u;
    uint32_t functionTable = 0u;
  };


  struct D3D11InterfaceSlot {
    small_vector<D3D11InterfaceType, 16> types;
  };


  struct D3D11InstanceData {
    uint32_t data = 0u;
    uint32_t functionTable = 0u;
  };


  class D3D11InterfaceInfo {

  public:

    D3D11InterfaceInfo();

    ~D3D11InterfaceInfo();

    D3D11InstanceData EncodeInstanceData(
            uint32_t                    SlotId,
            D3D11ClassInstance*         pInstance) const;

    void AddType(
            uint32_t                    TypeId,
      const char*                       pTypeName);

    void AddSlotInfo(
            uint32_t                    FirstSlot,
            uint32_t                    SlotCount,
            uint32_t                    TypeId,
            uint32_t                    FunctionTable);

    const char* GetTypeName(
            uint32_t                    TypeId);

  private:

    std::vector<std::string>        m_typeNames;
    std::vector<D3D11InterfaceSlot> m_interfaceSlots;

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

    D3D11InstanceData EncodeInstanceData(uint32_t Ft) const;

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
