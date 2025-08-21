//
//  avcodec.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/11.
//

#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace av {

namespace ffmpeg {

class CodecBase {
 public:
  CodecBase(const CodecBase&) = delete;
  CodecBase& operator=(const CodecBase&) = delete;

  CodecBase(CodecBase&& rhs) noexcept;

  CodecBase& operator=(CodecBase&& rhs) noexcept;

  virtual ~CodecBase();

  void open(AVDictionary** opts = nullptr);

  inline const AVCodec* codec() const { return codec_; }

  inline AVCodecContext* ctx() { return ctx_; }

 protected:
  explicit CodecBase(const AVCodec* codec);

  virtual void clean();

  const AVCodec* codec_ = nullptr;
  AVCodecContext* ctx_ = nullptr;
  bool is_open_ = false;
};

class Decoder : public CodecBase {
 public:
  Decoder(enum AVCodecID codec_id);

  Decoder(const char* codec_name);

  Decoder(Decoder&& rhs) = default;

  Decoder& operator=(Decoder&& rhs) = default;

  virtual ~Decoder() = default;

  int send_packet(const AVPacket* pkt);

  int receive_frame(AVFrame* frame);
};

class Encoder : public CodecBase {
 public:
  Encoder(enum AVCodecID codec_id);

  Encoder(const char* codec_name);

  Encoder(Encoder&& rhs) = default;

  Encoder& operator=(Encoder&& rhs) = default;

  virtual ~Encoder() = default;

  int send_frame(const AVFrame* frame);

  int receive_packet(AVPacket* pkt);
};

} // ffmpeg

} // av
