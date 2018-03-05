#pragma once

#include <ostream>
#include <iomanip>

#include "com_include.h"

#define DXVK_DEFINE_GUID(iface) \
  template<> inline GUID const& __mingw_uuidof<iface> () { return iface::guid; }

std::ostream& operator << (std::ostream& os, REFIID guid);
