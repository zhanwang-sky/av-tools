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

#define INPUT_FORMAT_NAME "alaw"
#define INPUT_SAMPLE_RATE "8000"
#define INPUT_CH_LAYOUT "mono"
#define TARGET_CODEC_ID AV_CODEC_ID_PCM_ALAW

int remux(const char* input_file, const char* output_file) {
  const AVInputFormat* ifmt = nullptr;
  AVDictionary* opts = nullptr;
  AVPacket* pkt = nullptr;
  int ret = 0;

  ifmt = av_find_input_format(INPUT_FORMAT_NAME);
  if (!ifmt) {
    cerr << "Could not find input format '" << INPUT_FORMAT_NAME << "'\n";
    ret = -1;
    goto exit;
  }

  // libavformat/pcmdec.c
  if ((av_dict_set(&opts, "sample_rate", INPUT_SAMPLE_RATE, 0) < 0) ||
      (av_dict_set(&opts, "ch_layout", INPUT_CH_LAYOUT, 0) < 0)) {
    ret = -2;
    goto exit;
  }

  pkt = av_packet_alloc();
  if (!pkt) {
    cerr << "Fail to alloc AVPacket\n";
    ret = -3;
    goto exit;
  }

  try {
    // Input Demuxer
    av::ffmpeg::AVDemuxer demuxer(input_file, ifmt, &opts);
    AVFormatContext* demux_ctx = demuxer.ctx();
    AVStream* ist = nullptr;

    for (unsigned i = 0; i != demux_ctx->nb_streams; ++i) {
      AVStream* st = demux_ctx->streams[i];
      if (st->codecpar->codec_id == TARGET_CODEC_ID) {
        ist = st;
        break;
      }
    }

    if (!ist) {
      ret = -4;
      throw std::runtime_error("Could not find target stream");
    }

    av_dump_format(demux_ctx, 0, input_file, 0);

    // Output Muxer
    av::ffmpeg::AVMuxer muxer(output_file);
    AVFormatContext* mux_ctx = muxer.ctx();
    AVStream* ost = muxer.new_stream();
    if (!ost) {
      ret = -5;
      throw std::runtime_error("Fail to create output stream");
    }

    ost->codecpar->codec_type = ist->codecpar->codec_type;
    ost->codecpar->codec_id = ist->codecpar->codec_id;
    ost->codecpar->format = ist->codecpar->format;
    av_channel_layout_copy(&ost->codecpar->ch_layout, &ist->codecpar->ch_layout);
    ost->codecpar->sample_rate = ist->codecpar->sample_rate;
    ost->time_base = ist->time_base;

    if (muxer.write_header() < 0) {
      ret = -6;
      throw std::runtime_error("Fail to write header");
    }

    av_dump_format(mux_ctx, 0, output_file, 1);

    // Remux
    unsigned samples = 0;

    for (;;) {
      int rc;
      char err_msg[AV_ERROR_MAX_STRING_SIZE];

      if ((rc = demuxer.read_frame(pkt)) < 0) {
        av_strerror(rc, err_msg, sizeof(err_msg));
        cerr << "Error reading frame: " << err_msg << endl;
        break;
      }

      if (pkt->stream_index != ist->index) {
        av_packet_unref(pkt);
        continue;
      }
      pkt->stream_index = ost->index;
      samples += pkt->pts;

      if ((rc = muxer.interleaved_write_frame(pkt)) < 0) {
        av_strerror(rc, err_msg, sizeof(err_msg));
        cerr << "Error writing frame: " << err_msg << endl;
        break;
      }
    }

    cout << samples << " samples remuxed\n";

  } catch (const std::exception& e) {
    cerr << "Exception caught: " << e.what() << endl;
  }

exit:
  av_packet_free(&pkt);
  av_dict_free(&opts);
  return ret;
}

int main(int argc, char* argv[]) {
  av::ffmpeg::AVDemuxer demuxer;
  av::ffmpeg::AVMuxer muxer;

  if (argc != 3) {
    cerr << "Usage: ./av-tools <input_file> <output_file>\n";
    exit(EXIT_FAILURE);
  }

  remux(argv[1], argv[2]);

  return 0;
}
