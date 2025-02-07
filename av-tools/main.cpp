//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cstdlib>
#include <iostream>

#include "ffmpeg/avformat.hpp"

using std::cout;
using std::cerr;
using std::endl;

int main(int argc, char* argv[]) {
  av::ffmpeg::AVOFormat ofmt;

  if (argc != 2 && argc != 3) {
    cerr << "Usage: ./av-tools <url> [fmt]\n";
    exit(EXIT_FAILURE);
  }

  try {
    if (argc > 2) {
      ofmt.open(argv[1], argv[2]);
    } else {
      ofmt.open(argv[1]);
    }
  } catch (std::exception &e) {
    cerr << "\nexception caught: " << e.what() << endl;
    exit(EXIT_FAILURE);
  }

  return 0;
}
