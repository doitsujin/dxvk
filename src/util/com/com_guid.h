#pragma once

#include <ostream>
#include <iomanip>

#include "com_include.h"

#ifndef _MSC_VER
#define DXVK_DEFINE_GUID(iface) \
  template<> inline GUID const& __mingw_uuidof<iface> () { return iface::guid; }
#endif

std::ostream& operator << (std::ostream& os, REFIID guid);
