//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cmath>
#include <iostream>
#include "av-tools/capi/av_streamer.h"

using std::cout;
using std::cerr;
using std::endl;

#define SAMPLE_RATE 8000
int16_t audio_buf[SAMPLE_RATE];

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cerr << "Usage: ./av-tools <output>\n";
    exit(EXIT_FAILURE);
  }

  auto p_streamer = av_streamer_alloc(SAMPLE_RATE, 1, argv[1]);
  if (!p_streamer) {
    cerr << "Fail to alloc av_streamer\n";
    exit(EXIT_FAILURE);
  }
  if (av_streamer_write_audio(p_streamer, (uint8_t*) audio_buf, SAMPLE_RATE) < 0) {
    cerr << "error writing audio";
  }
  av_streamer_free(p_streamer);

  return 0;
}
