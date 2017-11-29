#include "com_guid.h"

#include "../../d3d11/d3d11_interfaces.h"

#include "../../dxgi/dxgi_interfaces.h"

const GUID IDXGIAdapterPrivate::guid        = {0x907bf281,0xea3c,0x43b4,{0xa8,0xe4,0x9f,0x23,0x11,0x07,0xb4,0xff}};
const GUID IDXGIDevicePrivate::guid         = {0x7a622cf6,0x627a,0x46b2,{0xb5,0x2f,0x36,0x0e,0xf3,0xda,0x83,0x1c}};
const GUID IDXGIBufferResourcePrivate::guid = {0x5679becd,0x8547,0x4d93,{0x96,0xa1,0xe6,0x1a,0x1c,0xe7,0xef,0x37}};
const GUID IDXGIImageResourcePrivate::guid  = {0x1cfe6592,0x7de0,0x4a07,{0x8d,0xcb,0x45,0x43,0xcb,0xbc,0x6a,0x7d}};

const GUID ID3D11DevicePrivate::guid        = {0xab2a2a58,0xd2ac,0x4ca0,{0x9a,0xd9,0xa2,0x60,0xca,0xfa,0x00,0xc8}};

std::ostream& operator << (std::ostream& os, REFIID guid) {
  os.width(8);
  os << std::hex << guid.Data1 << '-';

  os.width(4);
  os << std::hex << guid.Data2 << '-';

  os.width(4);
  os << std::hex << guid.Data3 << '-';

  os.width(2);
  os << std::hex
     << static_cast<short>(guid.Data4[0])
     << static_cast<short>(guid.Data4[1])
     << '-'
     << static_cast<short>(guid.Data4[2])
     << static_cast<short>(guid.Data4[3])
     << static_cast<short>(guid.Data4[4])
     << static_cast<short>(guid.Data4[5])
     << static_cast<short>(guid.Data4[6])
     << static_cast<short>(guid.Data4[7]);
  return os;
}
