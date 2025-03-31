//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cstdlib>
#include <iostream>
#include "av_streamer.h"

using std::cout;
using std::cerr;
using std::endl;

constexpr unsigned samples_per_frame = 160; // 20ms@8kHz
unsigned char audio_buf[samples_per_frame << 1];

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cerr << "Usage: ./av-tools <rtmp://balabala.com/foo/bar>\n";
    exit(EXIT_FAILURE);
  }

  auto p_streamer = av_streamer_alloc(argv[1], 8000);
  if (!p_streamer) {
    cerr << "fail to alloc av_streamer\n";
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i != 20000; i += 20) {
    if (av_streamer_write_samples(p_streamer,
                                  audio_buf,
                                  samples_per_frame) < 0) {
      cerr << "error writing samples\n";
      break;
    }
  }

  av_streamer_free(p_streamer);

  return 0;
}
