#include <cstring>
#include <fstream>
#include <vector>

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
  
  if (argc < 5) {
    std::cerr << "Usage: hlsl-compiler target entrypoint input.hlsl output.dxbc" << std::endl;
    return 1;
  }
  
  const LPWSTR target       = argv[1];
  const LPWSTR entryPoint   = argv[2];
  const LPWSTR inputFile    = argv[3];
  const LPWSTR outputFile   = argv[4];
  
  std::ifstream ifile(str::fromws(inputFile), std::ios::binary);
  ifile.ignore(std::numeric_limits<std::streamsize>::max());
  std::streamsize length = ifile.gcount();
  ifile.clear();
  
  ifile.seekg(0, std::ios_base::beg);
  std::vector<char> hlslCode(length);
  ifile.read(hlslCode.data(), length);
  
  Com<ID3DBlob> binary;
  Com<ID3DBlob> errors;
  
  HRESULT hr = D3DCompile(
    hlslCode.data(),
    hlslCode.size(),
    "Shader", nullptr, nullptr,
    str::fromws(entryPoint).c_str(),
    str::fromws(target).c_str(),
    D3DCOMPILE_OPTIMIZATION_LEVEL3 |
    D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES,
    0, &binary, &errors);
  
  if (FAILED(hr)) {
    if (errors != nullptr)
      std::cerr << reinterpret_cast<const char*>(errors->GetBufferPointer()) << std::endl;
    return 1;
  }
  
  std::ofstream outputStream(str::fromws(outputFile), std::ios::binary | std::ios::trunc);
  outputStream.write(reinterpret_cast<const char*>(binary->GetBufferPointer()), binary->GetBufferSize());
  return 0;
}
