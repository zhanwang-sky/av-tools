//
//  avcodec.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/11.
//

#include <stdexcept>
#include "av-tools/ffmpeg/avcodec.hpp"

using namespace av::ffmpeg;

CodecBase::CodecBase(CodecBase&& rhs) noexcept
    : codec_(rhs.codec_),
      ctx_(rhs.ctx_)
{
  rhs.codec_ = nullptr;
  rhs.ctx_ = nullptr;
}

CodecBase& CodecBase::operator=(CodecBase&& rhs) noexcept {
  if (this != &rhs) {
    close();

    codec_ = rhs.codec_;
    ctx_ = rhs.ctx_;

    rhs.codec_ = nullptr;
    rhs.ctx_ = nullptr;
  }

  return *this;
}

CodecBase::~CodecBase() {
  close();
}

int CodecBase::open(AVDictionary** opts) {
  return avcodec_open2(ctx_, codec_, opts);
}

CodecBase::CodecBase(const AVCodec* codec)
{
  if (!(codec_ = codec)) {
    throw std::runtime_error("CodecBase: invalid argument");
  }

  if (!(ctx_ = avcodec_alloc_context3(codec))) {
    throw std::runtime_error("CodecBase: error allocating codec context");
  }
}

void CodecBase::close() {
  avcodec_free_context(&ctx_);
  codec_ = nullptr;
}

Decoder::Decoder(enum AVCodecID codec_id)
    : CodecBase(avcodec_find_decoder(codec_id)) { }

Decoder::Decoder(const char* codec_name)
    : CodecBase(avcodec_find_decoder_by_name(codec_name)) { }

int Decoder::send_packet(const AVPacket* pkt) {
  return avcodec_send_packet(ctx_, pkt);
}

int Decoder::receive_frame(AVFrame* frame) {
  return avcodec_receive_frame(ctx_, frame);
}

Encoder::Encoder(enum AVCodecID codec_id)
    : CodecBase(avcodec_find_encoder(codec_id)) { }

Encoder::Encoder(const char* codec_name)
    : CodecBase(avcodec_find_encoder_by_name(codec_name)) { }

int Encoder::send_frame(const AVFrame* frame) {
  return avcodec_send_frame(ctx_, frame);
}

int Encoder::receive_packet(AVPacket* pkt) {
  return avcodec_receive_packet(ctx_, pkt);
}
