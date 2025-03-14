//
//  helper.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/14.
//

#include <stdexcept>
#include "helper.hpp"

using namespace av::ffmpeg;

DecodeHelper::DecodeHelper(const char* filename)
    : demuxer_(filename) {
  AVFormatContext* demux_ctx = demuxer_.ctx();

  av_dump_format(demux_ctx, 0, filename, 0);

  for (unsigned i = 0; i != demux_ctx->nb_streams; ++i) {
    AVStream* st = demux_ctx->streams[i];

    auto& decoder = decoders_.emplace_back(st->codecpar->codec_id);
    AVCodecContext* decode_ctx = decoder.ctx();

    if (avcodec_parameters_to_context(decode_ctx, st->codecpar) < 0) {
      throw std::runtime_error("DecodeHelper: fail to copy codecpar to decoder");
    }
    decode_ctx->pkt_timebase = st->time_base;

    if ((decode_ctx->codec_type == AVMEDIA_TYPE_AUDIO) ||
        (decode_ctx->codec_type == AVMEDIA_TYPE_VIDEO)) {
      decoder.open();
    }
  }
}

int DecodeHelper::play(int stream_index, on_frame_cb&& on_frame) {
  AVPacket* pkt = nullptr;
  AVFrame* frame = nullptr;
  int rc = 0;
  int ret = 0;

  pkt = av_packet_alloc();
  frame = av_frame_alloc();
  if (!pkt || !frame) {
    goto exit;
  }

  for (;;) {
    // demux
    rc = demuxer_.read_frame(pkt);
    if (rc < 0) {
      if (rc != AVERROR_EOF) {
        ret = -1;
      }
      break;
    }
    if (pkt->stream_index != stream_index) {
      av_packet_unref(pkt);
      continue;
    }

    // decode PART I
    rc = decoders_[stream_index].send_packet(pkt);
    av_packet_unref(pkt);
    if (rc == AVERROR(EAGAIN)) {
      continue;
    } else if (rc < 0) {
      ret = -1;
      break;
    }

    // decode PART II
    for (;;) {
      rc = decoders_[stream_index].receive_frame(frame);
      if (rc < 0) {
        if (rc != AVERROR(EAGAIN)) {
          ret = -1;
        }
        break;
      }
      // user callback
      on_frame(frame);
    }
    // decode error
    if (ret < 0) {
      break;
    }
  }

  // error occurred
  if (ret < 0) {
    goto exit;
  }

  // flush PART I
  rc = decoders_[stream_index].send_packet(nullptr);
  if (rc < 0) {
    ret = -1;
    goto exit;
  }
  // flush PART II
  for (;;) {
    rc = decoders_[stream_index].receive_frame(frame);
    if (rc < 0) {
      if (rc != AVERROR_EOF) {
        ret = -1;
      }
      break;
    }
    // user callback
    on_frame(frame);
  }

exit:
  av_frame_free(&frame);
  av_packet_free(&pkt);
  return ret;
}
