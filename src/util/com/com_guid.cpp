#include "com_guid.h"

#include "../../dxgi/dxgi_interfaces.h"

const GUID IDXGIAdapterPrivate::guid     = {0x907bf281,0xea3c,0x43b4,{0xa8,0xe4,0x9f,0x23,0x11,0x07,0xb4,0xff}};
const GUID IDXGIDevicePrivate::guid      = {0x7a622cf6,0x627a,0x46b2,{0xb5,0x2f,0x36,0x0e,0xf3,0xda,0x83,0x1c}};

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
