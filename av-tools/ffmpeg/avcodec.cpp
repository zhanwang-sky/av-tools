//
//  avcodec.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/11.
//

#include <cassert>
#include <stdexcept>
#include <string>

#include "avcodec.hpp"

using namespace av::ffmpeg;

CodecBase::CodecBase(const AVCodec* codec) {
  if (!(codec_ = codec)) {
    throw std::runtime_error("CodecBase: Codec not found");
  }

  if (!(ctx_ = avcodec_alloc_context3(codec))) {
    throw std::runtime_error("CodecBase: Cannot allocate memory");
  }
}

CodecBase::CodecBase(CodecBase&& rhs) noexcept
    : codec_(rhs.codec_), ctx_(rhs.ctx_), is_open_(rhs.is_open_)
{
  rhs.codec_ = nullptr;
  rhs.ctx_ = nullptr;
  rhs.is_open_ = false;
}

CodecBase& CodecBase::operator=(CodecBase&& rhs) noexcept {
  if (this != &rhs) {
    clean();

    codec_ = rhs.codec_;
    ctx_ = rhs.ctx_;
    is_open_ = rhs.is_open_;

    rhs.codec_ = nullptr;
    rhs.ctx_ = nullptr;
    rhs.is_open_ = false;
  }

  return *this;
}

CodecBase::~CodecBase() {
  clean();
}

void CodecBase::clean() {
  is_open_ = false;
  avcodec_free_context(&ctx_);
}

void CodecBase::open(AVDictionary** opts) {
  int rc;

  assert(!is_open_);

  rc = avcodec_open2(ctx_, codec_, opts);
  if (rc < 0) {
    std::string err_str;
    char err_msg[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(rc, err_msg, sizeof(err_msg));
    err_str.append("CodecBase: ").append(err_msg);
    throw std::runtime_error(err_str);
  }

  is_open_ = true;
}

Decoder::Decoder(enum AVCodecID codec_id)
    : CodecBase(avcodec_find_decoder(codec_id)) { }

Decoder::Decoder(const char* codec_name)
    : CodecBase(avcodec_find_decoder_by_name(codec_name)) { }

int Decoder::send_packet(const AVPacket* pkt) {
  assert(is_open_);
  return avcodec_send_packet(ctx_, pkt);
}

int Decoder::receive_frame(AVFrame* frame) {
  assert(is_open_);
  return avcodec_receive_frame(ctx_, frame);
}

Encoder::Encoder(enum AVCodecID codec_id)
    : CodecBase(avcodec_find_encoder(codec_id)) { }

Encoder::Encoder(const char* codec_name)
    : CodecBase(avcodec_find_encoder_by_name(codec_name)) { }

int Encoder::send_frame(const AVFrame* frame) {
  assert(is_open_);
  return avcodec_send_frame(ctx_, frame);
}

int Encoder::receive_packet(AVPacket* pkt) {
  assert(is_open_);
  return avcodec_receive_packet(ctx_, pkt);
}
