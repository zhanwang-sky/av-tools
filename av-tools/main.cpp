//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <iostream>
#include "av_streamer.h"

using std::cout;
using std::cerr;
using std::endl;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cerr << "Usage: ./av-tools <output>\n";
    exit(EXIT_FAILURE);
  }

  auto p_streamer = av_streamer_alloc(argv[1], 8000, 2);
  av_streamer_free(p_streamer);

  return 0;
}
