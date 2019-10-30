#include "hb-fuzzer.hh"

#include <iostream>
#include <iterator>
#include <fstream>
#include <assert.h>

std::string FileToString(const std::string &Path) {
  /* TODO This silently passes if file does not exist.  Fix it! */
  std::ifstream T(Path.c_str());
  return std::string((std::istreambuf_iterator<char>(T)),
                     std::istreambuf_iterator<char>());
}

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    std::string s = FileToString(argv[i]);
    std::cout << argv[i] << std::endl;
    LLVMFuzzerTestOneInput((const unsigned char*)s.data(), s.size());
  }
}
