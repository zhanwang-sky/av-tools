//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cstdlib>
#include <iostream>

#include "ffmpeg/helper.hpp"

using std::cout;
using std::cerr;
using std::endl;

int test_decode_helper(const char* input_file) {
  try {
    av::ffmpeg::DecodeHelper decode_helper(input_file);
    unsigned nb_frames = 0;
    int rc = 0;

    rc = decode_helper.play(0, [&nb_frames](AVFrame* frame) {
      cout << "[" << ++nb_frames << "] pts=" << frame->pts
           << ", duration=" << frame->duration << endl;
    });

    cout << "done, total " << nb_frames << " frames, rc=" << rc << endl;

  } catch (const std::exception& e) {
    cerr << "Exception caught: " << e.what() << endl;
    return -1;
  }

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
