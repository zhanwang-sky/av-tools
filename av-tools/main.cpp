//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cstdlib>
#include <iostream>

#include "LiveStreamer.hpp"

using std::cout;
using std::cerr;
using std::endl;

int main(int argc, char* argv[]) {
  if (argc != 3) {
    cerr << "Usage: ./av-tools <sample.flv> <sample.ppm>\n";
    exit(EXIT_FAILURE);
  }

  try {
    LiveStreamer streamer(argv[1], argv[2]);

    streamer.start();

    cout << "press Enter to exit...";
    std::cin.get();

    streamer.stop();

  } catch (const std::exception& e) {
    cerr << "Exception caught: " << e.what() << endl;
    exit(EXIT_FAILURE);
  }

  return 0;
}
