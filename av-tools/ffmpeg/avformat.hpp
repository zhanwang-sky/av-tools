//
//  avformat.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#pragma once

extern "C" {
#include <libavformat/avformat.h>
}

namespace av {

namespace ffmpeg {

class Demuxer {
 public:
  Demuxer(const Demuxer&) = delete;
  Demuxer& operator=(const Demuxer&) = delete;

  Demuxer();

  Demuxer(Demuxer&& rhs) noexcept;

  Demuxer& operator=(Demuxer&& rhs) noexcept;

  virtual ~Demuxer();

  inline void set_avio(AVIOContext* avio) { ctx_->pb = avio; }

  inline AVFormatContext* ctx() { return ctx_; }

  int open(const char* url,
           const AVInputFormat* ifmt = nullptr,
           AVDictionary** opts = nullptr);

  int find_stream_info(AVDictionary** opts = nullptr);

  int read_frame(AVPacket* pkt);

 protected:
  void close();

 private:
  AVFormatContext* ctx_ = nullptr;
};

class Muxer {
 public:
  Muxer(const Muxer&) = delete;
  Muxer& operator=(const Muxer&) = delete;

  Muxer() = default;

  Muxer(Muxer&& rhs) noexcept;

  Muxer& operator=(Muxer&& rhs) noexcept;

  virtual ~Muxer();

  inline void set_avio(AVIOContext* avio) { avio_ = avio; }

  inline AVFormatContext* ctx() { return ctx_; }

  int open(const char* url,
           const char* fmt_name = nullptr,
           const AVOutputFormat* ofmt = nullptr);

  AVStream* new_stream();

  int write_header(AVDictionary** opts = nullptr);

  int write_frame(AVPacket* pkt);

  int interleaved_write_frame(AVPacket* pkt);

 protected:
  void close();

 private:
  AVIOContext* avio_ = nullptr;
  AVFormatContext* ctx_ = nullptr;
  bool need_close_ = false;
  bool need_trailer_ = false;
};

} // ffmpeg

} // av
