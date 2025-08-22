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

namespace av {

namespace ffmpeg {

struct DecodeHelper {
  using packet_callback = std::function<bool(AVPacket*)>;
  using frame_callback = std::function<void(unsigned int, AVFrame*)>;

  DecodeHelper(packet_callback&& pkt_cb,
               frame_callback&& frame_cb,
               const char* filename,
               const AVInputFormat* ifmt = nullptr,
               AVDictionary** opts = nullptr);

  virtual ~DecodeHelper() = default;

  int read();

  void flush(std::span<const unsigned int> ids);

  static constexpr auto pkt_deleter = [](AVPacket* pkt) { av_packet_free(&pkt); };
  static constexpr auto frame_deleter = [](AVFrame* frame) { av_frame_free(&frame); };

  packet_callback pkt_cb_;
  frame_callback frame_cb_;
  std::unique_ptr<AVPacket, decltype(pkt_deleter)> pkt_;
  std::unique_ptr<AVFrame, decltype(frame_deleter)> frame_;
  std::vector<Decoder> decoders_;
  Demuxer demuxer_;
};

struct EncodeHelper {
  using on_stream_cb = std::function<void(Muxer&, Encoder&, AVStream*, unsigned)>;
  using on_write_cb = std::function<bool(unsigned, AVFrame*, AVPacket*)>;

  static std::unique_ptr<EncodeHelper>
  FromCodecID(const char* filename,
              const std::vector<AVCodecID>& vid,
              on_stream_cb&& on_stream) noexcept(false);

  EncodeHelper(const char* filename);

  int write(unsigned stream_id, AVFrame* frame, on_write_cb&& on_write);

  static constexpr auto pkt_deleter = [](AVPacket* pkt) { av_packet_free(&pkt); };
  std::unique_ptr<AVPacket, decltype(pkt_deleter)> pkt_;
  std::vector<Encoder> encoders_;
  Muxer muxer_;
};

} // ffmpeg

} // av
