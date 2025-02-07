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

    AVStream* st = ofmt.new_stream();
    if (!st) {
      throw std::runtime_error("Fail to create new stream");
    }
    st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    st->codecpar->codec_id = AV_CODEC_ID_H264;
    st->codecpar->width = 1280;
    st->codecpar->height = 720;
    st->time_base = (AVRational) {1, 1000};

    if (ofmt.write_header() < 0) {
      throw std::runtime_error("Fail to write header");
    }

    cout << "\nactual timebase for st[" << st->index << "] is "
         << st->time_base.num << '/' << st->time_base.den
         << endl;
  } catch (std::exception &e) {
    cerr << "\nexception caught: " << e.what() << endl;
    exit(EXIT_FAILURE);
  }

  return 0;
}
