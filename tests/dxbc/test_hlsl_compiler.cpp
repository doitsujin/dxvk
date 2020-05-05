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
    std::cerr << "Usage: hlsl-compiler target entrypoint input.hlsl output.dxbc [--strip] [--text]" << std::endl;
    return 1;
  }

  bool strip = false;
  bool text = false;

  for (int i = 5; i < argc; i++) {
    strip |= str::fromws(argv[i]) == "--strip";
    text  |= str::fromws(argv[i]) == "--text";
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
  
  if (strip) {
    Com<ID3DBlob> strippedBlob;

    hr = D3DStripShader(binary->GetBufferPointer(), binary->GetBufferSize(),
      D3DCOMPILER_STRIP_REFLECTION_DATA | D3DCOMPILER_STRIP_DEBUG_INFO,
      &strippedBlob);

    if (FAILED(hr)) {
      std::cerr << "Failed to strip shader" << std::endl;
      return 1;
    }

    binary = strippedBlob;
  }

  std::ofstream file;

  if (str::fromws(outputFile) != "-")
    file = std::ofstream(str::fromws(outputFile), std::ios::binary | std::ios::trunc);

  std::ostream& outputStream = file.is_open() ? file : std::cout;

  if (text) {
    auto data = reinterpret_cast<const uint32_t*>(binary->GetBufferPointer());
    auto size = binary->GetBufferSize() / sizeof(uint32_t);

    outputStream << std::hex;

    for (uint32_t i = 0; i < size; i++) {
      if (i && !(i & 0x7))
        outputStream << std::endl;
      outputStream << "0x" << std::setfill('0') << std::setw(8) << data[i] << ", ";
    }

    outputStream << std::endl;
  } else {
    outputStream.write(reinterpret_cast<const char*>(binary->GetBufferPointer()), binary->GetBufferSize());
  }

  return 0;
}
