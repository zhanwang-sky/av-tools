//
//  remux.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/15.
//

#include <iostream>
#include <stdexcept>

#include "avformat.hpp"
#include "remux.hpp"

#define INPUT_FORMAT_NAME "alaw"
// options for pcm (defined in libavformat/pcmdec.c)
#define INPUT_SAMPLE_RATE "8000"
#define INPUT_CH_LAYOUT "mono"
#define TARGET_CODEC_ID AV_CODEC_ID_PCM_ALAW

using std::cout;
using std::cerr;
using std::endl;

int remux(const char* input_file, const char* output_file) {
  const AVInputFormat* input_fmt = nullptr;
  AVDictionary* input_opts = nullptr;
  AVPacket* pkt = nullptr;
  int ret = 0;

#ifdef INPUT_FORMAT_NAME
  input_fmt = av_find_input_format(INPUT_FORMAT_NAME);
  if (!input_fmt) {
    cerr << "Could not find input format '" << INPUT_FORMAT_NAME << "'\n";
    ret = -1;
    goto exit;
  }
#endif

  if (input_opts
#ifdef INPUT_SAMPLE_RATE
      || (av_dict_set(&input_opts, "sample_rate", INPUT_SAMPLE_RATE, 0) < 0)
#endif
#ifdef INPUT_CH_LAYOUT
      || (av_dict_set(&input_opts, "ch_layout", INPUT_CH_LAYOUT, 0) < 0)
#endif
      ) {
    cerr << "Fail to set input options\n";
    ret = -1;
    goto exit;
  }

  pkt = av_packet_alloc();
  if (!pkt) {
    cerr << "Fail to alloc AVPacket\n";
    ret = -1;
    goto exit;
  }

  try {
    // Input Demuxer
    av::ffmpeg::Demuxer demuxer(input_file, input_fmt, &input_opts);
    AVFormatContext* demux_ctx = demuxer.ctx();
    AVStream* ist = nullptr;

    av_dump_format(demux_ctx, 0, input_file, 0);

    for (unsigned i = 0; i != demux_ctx->nb_streams; ++i) {
      AVStream* st = demux_ctx->streams[i];
      if (st->codecpar->codec_id == TARGET_CODEC_ID) {
        ist = st;
        break;
      }
    }
    if (!ist) {
      throw std::runtime_error("Could not find target stream");
    }

    // Output Muxer
    av::ffmpeg::Muxer muxer(output_file);
    AVFormatContext* mux_ctx = muxer.ctx();
    AVStream* ost = nullptr;

    ost = muxer.new_stream();
    if (!ost) {
      throw std::runtime_error("Fail to create output stream");
    }
    avcodec_parameters_copy(ost->codecpar, ist->codecpar);
    ost->codecpar->codec_tag = 0;
    ost->time_base = ist->time_base; // inconclusive

    if (muxer.write_header() < 0) {
      throw std::runtime_error("Fail to write header");
    }

    av_dump_format(mux_ctx, 0, output_file, 1);

    cout << "Input timebase: "
         << ist->time_base.num << '/' << ist->time_base.den
         << endl;
    cout << "Output timebase: "
         << ost->time_base.num << '/' << ost->time_base.den
         << endl;

    // Remux
    unsigned packets = 0;

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
      av_packet_rescale_ts(pkt, ist->time_base, ost->time_base);
      pkt->pos = -1;

      if ((rc = muxer.interleaved_write_frame(pkt)) < 0) {
        av_strerror(rc, err_msg, sizeof(err_msg));
        cerr << "Error writing frame: " << err_msg << endl;
        break;
      }

      ++packets;
    }

    cout << packets << " packets remuxed\n";

  } catch (const std::exception& e) {
    cerr << "Exception caught: " << e.what() << endl;
    ret = -1;
  }

exit:
  av_packet_free(&pkt);
  av_dict_free(&input_opts);
  return ret;
}
