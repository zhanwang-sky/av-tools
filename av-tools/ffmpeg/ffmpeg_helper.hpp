//
//  ffmpeg_helper.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/14.
//

#pragma once

#include <functional>
#include <memory>
#include "av-tools/ffmpeg/avcodec.hpp"
#include "av-tools/ffmpeg/avformat.hpp"
#include "av-tools/ffmpeg/swresample.hpp"

namespace av {

namespace ffmpeg {

inline void frame_deleter(AVFrame* frame) { av_frame_free(&frame); }

inline void pkt_deleter(AVPacket* pkt) { av_packet_free(&pkt); }

struct EncodeHelper {
  using packet_callback = std::function<void(AVPacket*)>;

  EncodeHelper(enum AVCodecID codec_id, packet_callback&& pkt_cb);

  EncodeHelper(const char* codec_name, packet_callback&& pkt_cb);

  ~EncodeHelper() = default;

  int encode(const AVFrame* frame);

  Encoder encoder_;
  std::unique_ptr<AVPacket, decltype(&pkt_deleter)> pkt_;
  packet_callback pkt_cb_;
};

} // ffmpeg

} // av
