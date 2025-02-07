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
  av::ffmpeg::AVIFormat ifmt;
  av::ffmpeg::AVOFormat ofmt;

  if (argc != 3) {
    cerr << "Usage: ./av-tools <input.h264> <output.flv>\n";
    exit(EXIT_FAILURE);
  }

  try {
    AVPacket* pkt = nullptr;
    AVStream* input_stream = NULL;
    AVStream* output_stream = NULL;
    unsigned frame_cnt;

    pkt = av_packet_alloc();
    if (!pkt) {
      throw std::runtime_error("Fail to alloc AVPacket");
    }

    // IFormat
    ifmt.open(argv[1]);

    for (unsigned i = 0; i != ifmt.ctx()->nb_streams; ++i) {
      AVStream* st = ifmt.ctx()->streams[i];
      if ((st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) &&
          (st->codecpar->codec_id == AV_CODEC_ID_H264)) {
        input_stream = st;
        break;
      }
    }

    if (!input_stream) {
      throw std::runtime_error("No H264 stream found");
    }

    av_dump_format(ifmt.ctx(), 0, argv[1], 0);

    // OFormat
    ofmt.open(argv[2]);

    output_stream = ofmt.new_stream();
    if (!output_stream) {
      throw std::runtime_error("Fail to add output stream");
    }
    output_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    output_stream->codecpar->codec_id = AV_CODEC_ID_H264;
    output_stream->codecpar->width = input_stream->codecpar->width;
    output_stream->codecpar->height = input_stream->codecpar->height;
    output_stream->time_base = (AVRational) {1, 1000};

    if (ofmt.write_header() < 0) {
      throw std::runtime_error("Fail to write header");
    }

    // remux
    for (frame_cnt = 0; frame_cnt != 10000; ++frame_cnt) {
      if (ifmt.read_frame(pkt) < 0) {
        cerr << "Error reading frame\n";
        break;
      }
      pkt->pts = frame_cnt * 50 / 3;
      pkt->dts = pkt->pts;
      pkt->duration = 0;
      if (ofmt.write_frame(pkt) < 0) {
        cerr << "Error writing frame\n";
        break;
      }
    }

    cout << frame_cnt << " frames written\n";
  } catch (std::exception &e) {
    cerr << "exception caught: " << e.what() << endl;
    exit(EXIT_FAILURE);
  }

  return 0;
}
