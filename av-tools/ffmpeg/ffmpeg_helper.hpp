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

extern "C" {
#include <libavutil/dict.h>
}

namespace av {

namespace ffmpeg {

inline void frame_deleter(AVFrame* frame) { av_frame_free(&frame); }

inline void pkt_deleter(AVPacket* pkt) { av_packet_free(&pkt); }

struct DictHelper {
  DictHelper() = default;

  ~DictHelper() {
    av_dict_free(&dict_);
  }

  inline AVDictionary*& get() { return dict_; }

  AVDictionary* dict_ = nullptr;
};

struct ChannelLayoutHelper {
  ChannelLayoutHelper(int ac = 1) {
    av_channel_layout_default(&layout_, ac);
  }

  ~ChannelLayoutHelper() {
    av_channel_layout_uninit(&layout_);
  }

  inline const AVChannelLayout& get() const { return layout_; }

  AVChannelLayout layout_{};
};

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
