//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cstdlib>
#include <exception>
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

  while (transcode(argv[1], argv[2]) == 0) {
    cout << "enter to continue, press 'q' to exit\n";
    if (std::cin.get() == 'q') {
      break;
    }
  }

  return 0;
}
