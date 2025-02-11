//
//  avcodec.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/11.
//

#ifndef avcodec_hpp
#define avcodec_hpp

#include <cassert>
#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace av {

namespace ffmpeg {

class AVCodecBase {
 public:
  AVCodecBase(const AVCodecBase&) = delete;
  AVCodecBase& operator=(const AVCodecBase&) = delete;

  const AVCodec* codec() const {
    return codec_;
  }

  AVCodecContext* ctx() const {
    return ctx_;
  }

  void open(AVDictionary** opts = nullptr) {
    char err_msg[AV_ERROR_MAX_STRING_SIZE];
    int rc;

    assert(!is_open_);

    rc = avcodec_open2(ctx_, codec_, opts);
    if (rc < 0) {
      av_strerror(rc, err_msg, sizeof(err_msg));
      throw std::runtime_error(err_msg);
    }

    is_open_ = true;
  }

 protected:
  AVCodecBase(const AVCodec* codec) {
    const char* err_msg;

    if (!(codec_ = codec)) {
      err_msg = "Codec not found";
      goto err_exit;
    }

    if (!(ctx_ = avcodec_alloc_context3(codec_))) {
      err_msg = "Cannot allocate memory";
      goto err_exit;
    }

    return;

  err_exit:
    on_destruct();
    throw std::runtime_error(err_msg);
  }

  virtual ~AVCodecBase() {
    on_destruct();
  }

  bool is_open_ = false;
  const AVCodec* codec_ = nullptr;
  AVCodecContext* ctx_ = nullptr;

 private:
  inline void on_destruct() {
    avcodec_free_context(&ctx_);
    codec_ = nullptr;
    is_open_ = false;
  }
};

class AVDecoder : public AVCodecBase {
 public:
  AVDecoder(enum AVCodecID codec_id)
      : AVCodecBase(avcodec_find_decoder(codec_id)) { }

  AVDecoder(const char* codec_name)
      : AVCodecBase(avcodec_find_decoder_by_name(codec_name)) { }

  virtual ~AVDecoder() { }

  int send_packet(const AVPacket* pkt) {
    assert(is_open_);
    return avcodec_send_packet(ctx_, pkt);
  }

  int receive_frame(AVFrame* frame) {
    assert(is_open_);
    return avcodec_receive_frame(ctx_, frame);
  }
};

class AVEncoder : public AVCodecBase {
 public:
  AVEncoder(enum AVCodecID codec_id)
      : AVCodecBase(avcodec_find_encoder(codec_id)) { }

  AVEncoder(const char* codec_name)
      : AVCodecBase(avcodec_find_encoder_by_name(codec_name)) { }

  virtual ~AVEncoder() { }

  int send_frame(const AVFrame* frame) {
    assert(is_open_);
    return avcodec_send_frame(ctx_, frame);
  }

  int receive_packet(AVPacket* pkt) {
    assert(is_open_);
    return avcodec_receive_packet(ctx_, pkt);
  }
};

} // ffmpeg

} // av

#endif /* avcodec_hpp */
