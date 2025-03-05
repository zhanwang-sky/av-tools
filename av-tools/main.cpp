//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cstdlib>
#include <iostream>

#include "transcode.hpp"

using std::cout;
using std::cerr;
using std::endl;

int main(int argc, char* argv[]) {
  if (argc != 3) {
    cerr << "Usage: ./av-tools <input_file> <output_file>\n";
    exit(EXIT_FAILURE);
  }

  transcode(argv[1], argv[2]);

  return 0;
}
