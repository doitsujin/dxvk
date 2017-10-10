#include "com_guid.h"

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
