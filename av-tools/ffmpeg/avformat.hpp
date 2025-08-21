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

  Demuxer(const char* url,
          const AVInputFormat* ifmt = nullptr,
          AVDictionary** opts = nullptr);

  Demuxer(Demuxer&& rhs) noexcept;

  Demuxer& operator=(Demuxer&& rhs) noexcept;

  virtual ~Demuxer();

  inline AVFormatContext* ctx() { return ctx_; }

  int read_frame(AVPacket* pkt);

 private:
  void clean();

  AVFormatContext* ctx_ = nullptr;
};

class Muxer {
 public:
  Muxer(const Muxer&) = delete;
  Muxer& operator=(const Muxer&) = delete;

  Muxer(const char* url,
        const char* fmt_name = nullptr,
        const AVOutputFormat* ofmt = nullptr);

  Muxer(Muxer&& rhs) noexcept;

  Muxer& operator=(Muxer&& rhs) noexcept;

  virtual ~Muxer();

  inline AVFormatContext* ctx() { return ctx_; }

  AVStream* new_stream();

  int write_header(AVDictionary** opts = nullptr);

  int write_frame(AVPacket* pkt);

  int interleaved_write_frame(AVPacket* pkt);

 private:
  void clean();

  AVFormatContext* ctx_ = nullptr;
  bool need_close_ = false;
  bool need_trailer_ = false;
};

} // ffmpeg

} // av
