#pragma once

#include "d3d11_device.hpp"
#include "d3d11_private.h"

#include "../util/com/com_private_data.h"

namespace dxmt {

template <typename Base> class MTLD3D11DeviceObject : public Base {

public:
  MTLD3D11DeviceObject(IMTLD3D11Device *pDevice) : m_parent(pDevice) {}

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize,
                                           void *pData) final {
    return m_privateData.getData(guid, pDataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize,
                                           const void *pData) final {
    return m_privateData.setData(guid, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE
  SetPrivateDataInterface(REFGUID guid, const IUnknown *pUnknown) final {
    return m_privateData.setInterface(guid, pUnknown);
  }

  void STDMETHODCALLTYPE GetDevice(ID3D11Device **ppDevice) final {
    *ppDevice = ref(GetParentInterface());
  }

protected:
  ID3D11Device *GetParentInterface() const {
    // We don't know the definition of ID3D11Device
    // here, because D3D11Device includes this file.
    return reinterpret_cast<ID3D11Device *>(m_parent);
  }

  IMTLD3D11Device *const m_parent;

private:
  ComPrivateData m_privateData;
};

template <typename Base>
class MTLD3D11DeviceChild : public MTLD3D11DeviceObject<ComObject<Base>> {

public:
  MTLD3D11DeviceChild(IMTLD3D11Device *pDevice)
      : MTLD3D11DeviceObject<ComObject<Base>>(pDevice) {}

  ULONG STDMETHODCALLTYPE AddRef() {
    uint32_t refCount = this->m_refCount++;
    if (unlikely(!refCount)) {
      this->AddRefPrivate();
      this->GetParentInterface()->AddRef();
    }

    return refCount + 1;
  }

  ULONG STDMETHODCALLTYPE Release() {
    uint32_t refCount = --this->m_refCount;
    if (unlikely(!refCount)) {
      auto *parent = this->GetParentInterface();
      this->ReleasePrivate();
      parent->Release();
    }
    return refCount;
  }
};

template <typename Base>
class MTLD3D11StateObject : public MTLD3D11DeviceObject<Base> {

public:
  MTLD3D11StateObject(IMTLD3D11Device *pDevice)
      : MTLD3D11DeviceObject<Base>(pDevice) {}

  ULONG STDMETHODCALLTYPE AddRef() {
    uint32_t refCount = this->m_refCount++;
    if (unlikely(!refCount))
      this->GetParentInterface()->AddRef();

    return refCount + 1;
  }

  ULONG STDMETHODCALLTYPE Release() {
    uint32_t refCount = --this->m_refCount;
    if (unlikely(!refCount))
      this->GetParentInterface()->Release();

    return refCount;
  }

private:
  std::atomic<uint32_t> m_refCount = {0u};
};

} // namespace dxmt
