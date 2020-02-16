#include <cstring>
#include <iostream>
#include <sstream>
#include <windows.h>

#include "../test_utils.h"

using namespace dxvk;

// Basically just guesswork, but it appears they return a UINT64 (or *something* of that size)
// and the return value is consistent when calling it this way.
// This was consistent across tests on x86 and x64, so it isn't a SIZE_T.
//
// Using a debugger:
// Calling this function on it's own on x86 modifies both eax and edx
// whereas on x64 it only modifies all of rax to 0xa000100041770
// which matches the values of eax and edx which is consistent to some
// function returing a UINT64 value across these arches.
using pfnD3D10CoreGetVersion = UINT64(_stdcall *)();

// Calling this function as a HRESULT type, gives us a E_NOTIMPL value
// 99% confident this is at least the correct return type given that.
// No idea what it takes in though.
using pfnD3D10CoreRegisterLayers = HRESULT(_stdcall*)();
  
int main(int argc, char** argv) {
  HMODULE hD3D10     = LoadLibraryA("d3d10.dll");
  HMODULE hD3D10Core = LoadLibraryA("d3d10core.dll");
  HMODULE hD3D10_1   = LoadLibraryA("d3d10_1.dll");

  auto* D3D10GetVersion = reinterpret_cast<pfnD3D10CoreGetVersion>(
    GetProcAddress(hD3D10, "D3D10GetVersion"));

  auto* D3D10CoreGetVersion = reinterpret_cast<pfnD3D10CoreGetVersion>(
    GetProcAddress(hD3D10Core, "D3D10CoreGetVersion"));

  auto* D3D10GetVersion1 = reinterpret_cast<pfnD3D10CoreGetVersion>(
    GetProcAddress(hD3D10_1, "D3D10GetVersion"));

  // x86:
  // edx -> 0x000a0001
  // eax -> 0x00041770
  // x64:
  // rax -> 0xa000100041770
  UINT64 version = D3D10GetVersion();

  std::cout << "(d3d10.dll) D3D10GetVersion: " << std::hex << D3D10GetVersion() << std::endl;

  std::cout << "(d3d10core.dll) D3D10CoreGetVersion: " << std::hex << D3D10CoreGetVersion() << std::endl;

  std::cout << "(d3d10_1.dll) D3D10GetVersion: " << std::hex << D3D10GetVersion1() << std::endl;


  std::cout << std::endl;


  auto* D3D10RegisterLayers = reinterpret_cast<pfnD3D10CoreRegisterLayers>(
    GetProcAddress(hD3D10, "D3D10RegisterLayers"));

  auto* D3D10CoreRegisterLayers = reinterpret_cast<pfnD3D10CoreRegisterLayers>(
    GetProcAddress(hD3D10Core, "D3D10CoreRegisterLayers"));

  auto* D3D10RegisterLayers1 = reinterpret_cast<pfnD3D10CoreRegisterLayers>(
    GetProcAddress(hD3D10_1, "D3D10RegisterLayers"));

  std::cout << "(d3d10.dll) D3D10RegisterLayers: " << std::hex << D3D10RegisterLayers() << std::endl;

  std::cout << "(d3d10core.dll) D3D10CoreRegisterLayers: " << std::hex << D3D10CoreRegisterLayers() << std::endl;

  std::cout << "(d3d10_1.dll) D3D10RegisterLayers: " << std::hex << D3D10RegisterLayers1() << std::endl;

  return 0;
}
