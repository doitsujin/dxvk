#pragma once

#include <ostream>
#include <iomanip>

#include "com_include.h"

#ifndef _MSC_VER
# ifdef __WINE__
#   define DXVK_DEFINE_GUID(iface) \
      template<> inline GUID const& __wine_uuidof<iface> () { return iface::guid; }
# else
#   define DXVK_DEFINE_GUID(iface) \
      template<> inline GUID const& __mingw_uuidof<iface> () { return iface::guid; }
# endif
#endif

std::ostream& operator << (std::ostream& os, REFIID guid);
