//
//  ffmpeg_helper.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/14.
//

#include <stdexcept>
#include "ffmpeg_helper.hpp"

using namespace av;

std::unique_ptr<EncodeHelper>
EncodeHelper::FromCodecID(const char* filename,
                          const std::vector<AVCodecID>& ids,
                          on_new_stream_cb&& on_new_stream) {
  try {
    auto p_helper = std::make_unique<EncodeHelper>(filename);
    auto& muxer = p_helper->muxer_;

    for (std::size_t i = 0; i != ids.size(); ++i) {
      auto& encoder = p_helper->encoders_.emplace_back(ids[i]);
      AVStream* st = muxer.new_stream();
      if (!st) {
        throw;
      }
      on_new_stream(muxer, encoder, st, i);
    }

    return p_helper;

  } catch (...) {
    return nullptr;
  }
}

EncodeHelper::EncodeHelper(const char* filename)
    : pkt_(av_packet_alloc(), pkt_deleter), muxer_(filename) {
  if (!pkt_) {
    throw std::runtime_error("EncodeHelper: fail to alloc packet");
  }
}

int EncodeHelper::write_frame(std::size_t stream_id,
                              AVFrame* frame,
                              on_write_cb&& on_write) {
  auto& encoder = encoders_[stream_id];
  int rc = 0;

  rc = encoder.send_frame(frame);
  if (rc < 0) {
    return -1;
  }

  for (;;) {
    rc = encoder.receive_packet(pkt_.get());
    if ((rc == AVERROR(EAGAIN)) || (rc == AVERROR_EOF)) {
      return 0;
    } else if (rc < 0) {
      return -1;
    }

    if (!on_write(stream_id, frame, pkt_.get())) {
      // No matter whether the user unref the pkt or not,
      // it will be unref on the next call to receive_packet().
      continue;
    }

    rc = muxer_.interleaved_write_frame(pkt_.get());
    if (rc < 0) {
      // The returned packet will be blank, even on error. no need to unref
      return -1;
    }
  }

  return 0;
}
