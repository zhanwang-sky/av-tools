//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cstdlib>
#include <exception>
#include <iostream>

#include "ffmpeg_helper.hpp"

using std::cout;
using std::cerr;
using std::endl;

int test_decode_helper(const char* input_file) {
  // todo
  return 0;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cerr << "Usage: ./av-tools <input_file>\n";
    exit(EXIT_FAILURE);
  }

  while (test_decode_helper(argv[1]) == 0) {
    cout << "enter to continue, press 'q' to exit\n";
    if (std::cin.get() == 'q') {
      break;
    }
  }

  return 0;
}
