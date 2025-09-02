//
//  ffmpeg_helper.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/14.
//

#pragma once

#include <functional>
#include <memory>
#include <span>
#include <vector>
#include "avcodec.hpp"
#include "avformat.hpp"
#include "swresample.hpp"

namespace av {

namespace ffmpeg {

inline void frame_deleter(AVFrame* frame) { av_frame_free(&frame); }

inline void pkt_deleter(AVPacket* pkt) { av_packet_free(&pkt); }

struct DecodeHelper {
  using packet_callback = std::function<bool(AVPacket*)>;
  using frame_callback = std::function<void(unsigned int, AVFrame*)>;

  DecodeHelper(packet_callback&& pkt_cb,
               frame_callback&& frame_cb,
               const char* filename,
               const AVInputFormat* ifmt = nullptr,
               AVDictionary** opts = nullptr);

  ~DecodeHelper() = default;

  int read();

  void flush(std::span<const unsigned int> ids);

  packet_callback pkt_cb_;
  frame_callback frame_cb_;
  std::unique_ptr<AVPacket, decltype(&pkt_deleter)> pkt_;
  std::unique_ptr<AVFrame, decltype(&frame_deleter)> frame_;
  std::vector<Decoder> decoders_;
  Demuxer demuxer_;
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
