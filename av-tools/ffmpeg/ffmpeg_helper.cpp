//
//  ffmpeg_helper.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/14.
//

#include <stdexcept>
#include <utility>
#include "av-tools/ffmpeg/ffmpeg_helper.hpp"

using namespace av::ffmpeg;

EncodeHelper::EncodeHelper(enum AVCodecID codec_id, packet_callback&& pkt_cb)
    : encoder_(codec_id),
      pkt_(av_packet_alloc(), &pkt_deleter),
      pkt_cb_(std::move(pkt_cb))
{
  if (!pkt_) {
    throw std::runtime_error("EncodeHelper: Cannot allocate memory");
  }
}

EncodeHelper::EncodeHelper(const char* codec_name, packet_callback&& pkt_cb)
    : encoder_(codec_name),
      pkt_(av_packet_alloc(), &pkt_deleter),
      pkt_cb_(std::move(pkt_cb))
{
  if (!pkt_) {
    throw std::runtime_error("EncodeHelper: Cannot allocate memory");
  }
}

int EncodeHelper::encode(const AVFrame* frame) {
  int rc = encoder_.send_frame(frame);
  if (rc < 0) {
    return rc;
  }

  for (;;) {
    rc = encoder_.receive_packet(pkt_.get());
    if (rc < 0) {
      if ((rc == AVERROR(EAGAIN)) || (rc == AVERROR_EOF)) {
        rc = 0;
      }
      break;
    }
    pkt_cb_(pkt_.get());
  }

  return rc;
}
