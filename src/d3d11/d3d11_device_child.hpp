#pragma once

#include "d3d11_device.hpp"
#include "com/com_object.hpp"
#include "com/com_private_data.hpp"

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
    return reinterpret_cast<ID3D11Device *>(m_parent);
  }

  IMTLD3D11Device *const m_parent;

private:
  ComPrivateData m_privateData;
};

/**
In DXVK, DeviceChild hold a strong reference of Device
if it has public reference count > 0

At the moment, I prevent holding a strong reference to Device
in all internal interfaces (DeviceChild) expect for swapchain

So that the Device can use strong reference to any other interfaces
without circular reference. And yes the Device interface can be disposed
before DeviceChild. This should be OK since the document says:

> Direct3D 10 or later has slightly modified lifetime rules for objects. 
> In particular, objects that are derived from ID3DxxDeviceChild never 
> outlive their parent device (that is, if the owning ID3DxxDevice hits 
> a 0 refcount, then all child objects are immediately invalid as well)

see https://learn.microsoft.com/en-us/windows/win32/prog-dx-with-com

(by the way, the next statement "Also, when you use Set methods to bind 
objects to the render pipeline, these references don't increase the 
reference count (that is, they are weak references)." is false in DX11)

Speaking of multi-threading:
- might be appropriate to let CommandChunk hold a strong ref
- should double check ThreadgroupWork 

*/

template <typename... Base>
class MTLD3D11DeviceChild : public MTLD3D11DeviceObject<ComObject<Base...>> {

public:
  MTLD3D11DeviceChild(IMTLD3D11Device *pDevice)
      : MTLD3D11DeviceObject<ComObject<Base...>>(pDevice) {}

  // ULONG STDMETHODCALLTYPE AddRef() {
  //   uint32_t refCount = this->m_refCount++;
  //   if (unlikely(!refCount)) {
  //     this->AddRefPrivate();
  //     // this->GetParentInterface()->AddRef();
  //   }

  //   return refCount + 1;
  // }

  // ULONG STDMETHODCALLTYPE Release() {
  //   uint32_t refCount = --this->m_refCount;
  //   if (unlikely(!refCount)) {
  //     // auto *parent = this->GetParentInterface();
  //     this->ReleasePrivate();
  //     // parent->Release();
  //   }
  //   return refCount;
  // }
};

template <typename Base>
class MTLD3D11StateObject : public MTLD3D11DeviceObject<ComObject<Base>> {

public:
  MTLD3D11StateObject(IMTLD3D11Device *pDevice)
      : MTLD3D11DeviceObject<ComObject<Base>>(pDevice) {}

  // ULONG STDMETHODCALLTYPE AddRef() {
  //   uint32_t refCount = this->m_refCount++;
  //   // if (unlikely(!refCount))
  //   //   this->GetParentInterface()->AddRef();

  //   return refCount + 1;
  // }

  // ULONG STDMETHODCALLTYPE Release() {
  //   uint32_t refCount = --this->m_refCount;
  //   // if (unlikely(!refCount))
  //   //   this->GetParentInterface()->Release();

  //   return refCount;
  // }

  // private:
  // std::atomic<uint32_t> m_refCount = {0u};
};

} // namespace dxmt
