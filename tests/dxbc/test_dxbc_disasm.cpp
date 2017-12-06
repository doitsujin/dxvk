#include <cstring>
#include <fstream>

#include <d3dcompiler.h>

#include <shellapi.h>
#include <windows.h>
#include <windowsx.h>

#include "../test_utils.h"

using namespace dxvk;

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
  int     argc = 0;
  LPWSTR* argv = CommandLineToArgvW(
    GetCommandLineW(), &argc);  
  
  if (argc < 2) {
    std::cerr << "Usage: dxbc-disasm input.dxbc" << std::endl;
    return 1;
  }
  
  std::ifstream ifile(str::fromws(argv[1]), std::ios::binary);
  ifile.ignore(std::numeric_limits<std::streamsize>::max());
  std::streamsize length = ifile.gcount();
  ifile.clear();
  
  ifile.seekg(0, std::ios_base::beg);
  std::vector<char> dxbcCode(length);
  ifile.read(dxbcCode.data(), length);
  
  Com<ID3DBlob> assembly;
  
  HRESULT hr = D3DDisassemble(
    dxbcCode.data(),
    dxbcCode.size(),
    D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING, nullptr,
    &assembly);
  
  if (FAILED(hr)) {
    std::cerr << "Failed to disassemble shader" << std::endl;
    return 1;
  }
  
  std::string data;
  data.resize(assembly->GetBufferSize());
  std::memcpy(data.data(), assembly->GetBufferPointer(), data.size());
  std::cout << data;
  return 0;
}
