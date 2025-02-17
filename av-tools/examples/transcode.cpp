//
//  transcode.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/17.
//

#include <iostream>
#include <stdexcept>

#include "avcodec.hpp"
#include "avformat.hpp"
#include "transcode.hpp"

#define INPUT_FORMAT_NAME "alaw"
// options for pcm (defined in libavformat/pcmdec.c)
#define INPUT_SAMPLE_RATE "8000"
#define INPUT_CH_LAYOUT "mono"
#define INPUT_CODEC_ID AV_CODEC_ID_PCM_ALAW
#define OUTPUT_CODEC_ID AV_CODEC_ID_PCM_S16LE

using std::cout;
using std::cerr;
using std::endl;

int transcode(const char* input_file, const char* output_file) {
  const AVInputFormat* ifmt = nullptr;
  AVDictionary* opts = nullptr;
  AVPacket* input_pkt = nullptr;
  AVPacket* output_pkt = nullptr;
  AVFrame* frame = nullptr;
  int ret = 0;

#ifdef INPUT_FORMAT_NAME
  ifmt = av_find_input_format(INPUT_FORMAT_NAME);
  if (!ifmt) {
    cerr << "Could not find input format '" << INPUT_FORMAT_NAME << "'\n";
    ret = -1;
    goto exit;
  }
#endif

  if (opts
#ifdef INPUT_SAMPLE_RATE
      || (av_dict_set(&opts, "sample_rate", INPUT_SAMPLE_RATE, 0) < 0)
#endif
#ifdef INPUT_CH_LAYOUT
      || (av_dict_set(&opts, "ch_layout", INPUT_CH_LAYOUT, 0) < 0)
#endif
      ) {
    cerr << "Fail to set format options for input file\n";
    ret = -1;
    goto exit;
  }

  input_pkt = av_packet_alloc();
  output_pkt = av_packet_alloc();
  frame = av_frame_alloc();
  if (!input_pkt || !output_pkt || !frame) {
    cerr << "Fail to alloc FFmpeg objects\n";
    ret = -1;
    goto exit;
  }

  try {
    // Input Demuxer
    av::ffmpeg::AVDemuxer demuxer(input_file, ifmt, &opts);
    AVFormatContext* demux_ctx = demuxer.ctx();
    AVStream* ist = nullptr;

    av_dump_format(demux_ctx, 0, input_file, 0);

    for (unsigned i = 0; i != demux_ctx->nb_streams; ++i) {
      AVStream* st = demux_ctx->streams[i];
      if (st->codecpar->codec_id == INPUT_CODEC_ID) {
        ist = st;
        break;
      }
    }
    if (!ist) {
      throw std::runtime_error("Could not find specified input stream");
    }

    // Decoder
    av::ffmpeg::AVDecoder decoder(ist->codecpar->codec_id);
    AVCodecContext* decode_ctx = decoder.ctx();

    if (avcodec_parameters_to_context(decode_ctx, ist->codecpar) < 0) {
      throw std::runtime_error("Fail to copy codecpar to decoder");
    }
    decode_ctx->pkt_timebase = ist->time_base;

    decoder.open();

    // Encoder
    av::ffmpeg::AVEncoder encoder(OUTPUT_CODEC_ID);
    AVCodecContext* encode_ctx = encoder.ctx();

    if (encode_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
      // XXX TODO
    } else if (encode_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
      encode_ctx->time_base = (AVRational) {1, decode_ctx->sample_rate};
      encode_ctx->sample_rate = decode_ctx->sample_rate;
      encode_ctx->sample_fmt = decode_ctx->sample_fmt;
      if (av_channel_layout_copy(&encode_ctx->ch_layout,
                                 &decode_ctx->ch_layout) < 0) {
        throw std::runtime_error("Fail to copy ch_layout");
      }
    }

    // Output Muxer
    av::ffmpeg::AVMuxer muxer(output_file);
    AVFormatContext* mux_ctx = muxer.ctx();
    AVStream* ost = nullptr;

    ost = muxer.new_stream();
    if (!ost) {
      throw std::runtime_error("Fail to create output stream");
    }

    // Encoder & Output Stream
    if (mux_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
      encode_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    encoder.open();

    if (avcodec_parameters_from_context(ost->codecpar, encode_ctx) < 0) {
      throw std::runtime_error("Fail to copy codecpar from encoder");
    }
    ost->time_base = encode_ctx->time_base;

    // Output Muxer
    if (muxer.write_header() < 0) {
      throw std::runtime_error("Fail to write header");
    }

    av_dump_format(mux_ctx, 0, output_file, 1);

    // Transcode
    // packet -> frames[]
    // frame -> packets[]

    // stats
    unsigned pkt_decoded = 0;
    unsigned pkt_encoded = 0;

    // packet manipulators
    auto on_input_pkt = [&pkt_decoded](AVPacket* pkt) {
      ++pkt_decoded;
    };

    auto on_output_pkt = [&pkt_encoded, ost](AVPacket* pkt) {
      ++pkt_encoded;
      pkt->stream_index = ost->index;
    };

    // wrappers
    auto frame2packets = [&encoder, &muxer, &output_pkt, &on_output_pkt](AVFrame* frame) -> int {
      int rc;

      rc = encoder.send_frame(frame);

      while (rc >= 0) {
        rc = encoder.receive_packet(output_pkt);
        if (rc < 0) {
          if ((rc == AVERROR(EAGAIN)) || (rc == AVERROR_EOF)) {
            rc = 0;
          }
          break;
        }
        on_output_pkt(output_pkt);
        rc = muxer.interleaved_write_frame(output_pkt);
      }

      return rc;
    };

    auto packet2frames = [&decoder, &frame, &frame2packets](AVPacket* pkt) -> int {
      int rc;

      rc = decoder.send_packet(pkt);

      while (rc >= 0) {
        rc = decoder.receive_frame(frame);
        if (rc < 0) {
          if ((rc == AVERROR(EAGAIN)) || (rc == AVERROR_EOF)) {
            rc = 0;
          }
          break;
        }
        rc = frame2packets(frame);
      }

      return rc;
    };

    // do transcode
    int rc;

    for (;;) {
      if ((rc = demuxer.read_frame(input_pkt)) < 0) {
        break;
      }
      on_input_pkt(input_pkt);
      if ((rc = packet2frames(input_pkt)) < 0) {
        break;
      }
    }

    if (rc == AVERROR_EOF) {
      packet2frames(nullptr);
      frame2packets(nullptr);
    } else if (rc < 0) {
      char err_msg[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(rc, err_msg, sizeof(err_msg));
      cerr << "Error while transcoding: " << err_msg << endl;
    }

    // print stats
    cout << pkt_decoded << " packets decoded, " << pkt_encoded << " packets encoded\n";

  } catch (const std::exception& e) {
    cerr << "Exception caught: " << e.what() << endl;
    ret = -1;
  }

exit:
  av_frame_free(&frame);
  av_packet_free(&output_pkt);
  av_packet_free(&input_pkt);
  av_dict_free(&opts);
  return ret;
}
