//
//  transcode.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/17.
//

#include <cassert>
#include <iostream>
#include <stdexcept>

#include "ffmpeg_helper.hpp"
#include "transcode.hpp"

//#define INPUT_FORMAT_NAME "alaw"
// options for pcm (defined in libavformat/pcmdec.c)
//#define INPUT_SAMPLE_RATE "8000"
//#define INPUT_CH_LAYOUT "mono"
#define INPUT_CODEC_ID AV_CODEC_ID_H264
#define OUTPUT_CODEC_ID AV_CODEC_ID_H265
#define OUTPUT_BITRATE 10485760

using namespace av::ffmpeg;

using std::cout;
using std::cerr;
using std::endl;

int transcode(const char* input_file, const char* output_file) {
  const AVInputFormat* input_fmt = nullptr;
  AVDictionary* input_opts = nullptr;

#ifdef INPUT_FORMAT_NAME
  input_fmt = av_find_input_format(INPUT_FORMAT_NAME);
  if (!input_fmt) {
    cerr << "Could not find input format '" << INPUT_FORMAT_NAME << "'\n";
    return -1;
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
    av_dict_free(&input_opts);
    cerr << "Fail to set input options\n";
    return -1;
  }

  int ret = 0;

  try {
    // input file
    DecodeHelper decode_helper(input_file, input_fmt, &input_opts);
    auto& demuxer = decode_helper.demuxer_;
    AVFormatContext* demux_ctx = demuxer.ctx();

    av_dump_format(demux_ctx, 0, input_file, 0);

    // find target stream
    AVCodecContext* decode_ctx = nullptr;
    AVStream* ist = nullptr;

    for (unsigned i = 0; i != demux_ctx->nb_streams; ++i) {
      AVStream* st = demux_ctx->streams[i];
      if (st->codecpar->codec_id == INPUT_CODEC_ID) {
        decode_ctx = decode_helper.decoders_[i].ctx();
        ist = st;
        break;
      }
    }
    if (!ist) {
      throw std::runtime_error("Could not find target input stream");
    }

    cout << "ist->time_base: "
         << ist->time_base.num << '/' << ist->time_base.den
         << endl;

    // setup output stream
    AVStream* ost = nullptr;

    auto setup_ost = [decode_ctx, ist, &ost](Muxer& muxer,
                                             Encoder& encoder,
                                             AVStream* st,
                                             std::size_t) {
      AVFormatContext* mux_ctx = muxer.ctx();
      AVCodecContext* encode_ctx = encoder.ctx();

      assert(ost == nullptr);

      // encoder setup
      if (encode_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
#ifdef OUTPUT_BITRATE
        // average bitrate
        encode_ctx->bit_rate = OUTPUT_BITRATE;
#endif
        // basic video attributes
        encode_ctx->time_base = av_inv_q(decode_ctx->framerate);
        encode_ctx->framerate = decode_ctx->framerate;
        encode_ctx->width = decode_ctx->width;
        encode_ctx->height = decode_ctx->height;
        encode_ctx->pix_fmt = decode_ctx->pix_fmt;
      } else if (encode_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        encode_ctx->time_base = av_make_q(1, decode_ctx->sample_rate);
        encode_ctx->sample_rate = decode_ctx->sample_rate;
        encode_ctx->sample_fmt = decode_ctx->sample_fmt;
        if (av_channel_layout_copy(&encode_ctx->ch_layout,
                                   &decode_ctx->ch_layout) < 0) {
          throw std::runtime_error("Fail to copy ch_layout");
        }
      }
      if (mux_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        encode_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
      }
      encoder.open();

      cout << "encoder->time_base: "
           << encode_ctx->time_base.num << '/' << encode_ctx->time_base.den
           << endl;

      // stream setup
      if (avcodec_parameters_from_context(st->codecpar, encode_ctx) < 0) {
        throw std::runtime_error("Fail to copy codecpar from encoder");
      }
      st->time_base = ist->time_base; // inconclusive

      ost = st;
    };

    auto p_encode_helper = EncodeHelper::FromCodecID(output_file,
                                                     {OUTPUT_CODEC_ID},
                                                     setup_ost);
    if (!p_encode_helper) {
      throw std::runtime_error("Fail to create EncodeHelper");
    }

    assert(ost != nullptr);

    auto& muxer = p_encode_helper->muxer_;
    AVFormatContext* mux_ctx = muxer.ctx();

    if (muxer.write_header() < 0) {
      throw std::runtime_error("Fail to write header");
    }

    av_dump_format(mux_ctx, 0, output_file, 1);

    cout << "ost->time_base: "
         << ost->time_base.num << '/' << ost->time_base.den
         << endl;

    // transcode
    unsigned in_pkts = 0;
    unsigned out_pkts = 0;
    unsigned nb_frames = 0;
    int decode_rc = 0;
    int encode_rc = 0;

    auto on_write = [ist, ost, &out_pkts](unsigned st_id, AVFrame*, AVPacket* pkt) {
      if (pkt) {
        // pkt->duration =
        av_packet_rescale_ts(pkt, ist->time_base, ost->time_base);
        pkt->stream_index = st_id;
        pkt->pos = -1;
        ++out_pkts;
      }
      return true;
    };

    auto on_read = [&nb_frames, &p_encode_helper, &on_write, &encode_rc](unsigned, AVPacket*, AVFrame* frame) {
      if (frame) {
        if (encode_rc >= 0) {
          encode_rc = p_encode_helper->write(0, frame, on_write);
        }
        ++nb_frames;
      }
      return true;
    };

    // transcode loop
    while ((decode_rc = decode_helper.read(on_read)) > 0) {
      if (encode_rc < 0) {
        break;
      }
      in_pkts += decode_rc;
    }
    // flush decoder
    if ((decode_rc == 0) && (encode_rc >= 0)) {
      decode_rc = decode_helper.flush(0, on_read);
    }
    // flush encoder
    if ((decode_rc == 0) && (encode_rc >= 0)) {
      encode_rc = p_encode_helper->write(0, nullptr, on_write);
    }

    cout << "done, decode_rc=" << decode_rc << ", encode_rc=" << encode_rc << endl;
    cout << "total " << nb_frames << " frames (" << in_pkts << " input packets | " << out_pkts << " output packets)\n";

  } catch (const std::exception& e) {
    cerr << "Exception caught: " << e.what() << endl;
    ret = -1;
  } catch (...) {
    cerr << "Unknown exception caught!\n";
    ret = -1;
  }

  av_dict_free(&input_opts);

  return ret;
}
