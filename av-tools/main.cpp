//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cmath>
#include <iostream>
#include <thread>
#include "av-tools/capi/av_streamer.h"

using std::cout;
using std::cerr;
using std::endl;

#define PI 3.14159265
#define SAMPLE_RATE 16000
#define CENT_FREQ 950
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

  double t = 0.0;
  double p = 0.0;

  for (;;) {
    for (int i = 0; i != SAMPLE_RATE; ++i) {
      double f = sin(2.0 * PI * (t / 4.5)) * 400.0 + CENT_FREQ;
      audio_buf[i] = static_cast<int16_t>(sin(p) * INT16_MAX);
      p += 2.0 * PI * f / SAMPLE_RATE;
      t += 1.0 / SAMPLE_RATE;
    }
    if (av_streamer_write_audio(p_streamer, reinterpret_cast<const uint8_t*>(audio_buf), SAMPLE_RATE) < 0) {
      break;
    }
    cout << "samples write\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(990));
  }

  av_streamer_free(p_streamer);

  cout << "terminated\n";

  return 0;
}
