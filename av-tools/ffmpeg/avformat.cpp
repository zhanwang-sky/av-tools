//
//  avformat.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <stdexcept>
#include "av-tools/ffmpeg/avformat.hpp"

using namespace av::ffmpeg;

Demuxer::Demuxer()
{
  if (!(ctx_ = avformat_alloc_context())) {
    throw std::runtime_error("Demuxer: error allocating format context");
  }
}

Demuxer::Demuxer(Demuxer&& rhs) noexcept
    : ctx_(rhs.ctx_)
{
  rhs.ctx_ = nullptr;
}

Demuxer& Demuxer::operator=(Demuxer&& rhs) noexcept {
  if (this != &rhs) {
    close();
    ctx_ = rhs.ctx_;
    rhs.ctx_ = nullptr;
  }
  return *this;
}

Demuxer::~Demuxer() {
  close();
}

int Demuxer::open(const char* url,
                  const AVInputFormat* ifmt,
                  AVDictionary** opts) {
  return avformat_open_input(&ctx_, url, ifmt, opts);
}

int Demuxer::find_stream_info(AVDictionary** opts) {
  return avformat_find_stream_info(ctx_, opts);
}

int Demuxer::read_frame(AVPacket* pkt) {
  return av_read_frame(ctx_, pkt);
}

void Demuxer::close() {
  avformat_close_input(&ctx_);
}

Muxer::Muxer()
{
  if (!(ctx_ = avformat_alloc_context())) {
    throw std::runtime_error("Muxer: error allocating format context");
  }
}

Muxer::Muxer(Muxer&& rhs) noexcept
    : ctx_(rhs.ctx_),
      need_close_(rhs.need_close_),
      need_trailer_(rhs.need_trailer_)
{
  rhs.ctx_ = nullptr;
  rhs.need_close_ = false;
  rhs.need_trailer_ = false;
}

Muxer& Muxer::operator=(Muxer&& rhs) noexcept {
  if (this != &rhs) {
    close();

    ctx_ = rhs.ctx_;
    need_close_ = rhs.need_close_;
    need_trailer_ = rhs.need_trailer_;

    rhs.ctx_ = nullptr;
    rhs.need_close_ = false;
    rhs.need_trailer_ = false;
  }

  return *this;
}

Muxer::~Muxer() {
  close();
}

int Muxer::open(const char* url,
                const char* fmt_name,
                const AVOutputFormat* ofmt) {
  if (!ofmt) {
    if (fmt_name) {
      ofmt = av_guess_format(fmt_name, nullptr, nullptr);
    } else {
      ofmt = av_guess_format(nullptr, url, nullptr);
    }
    if (!ofmt) {
      return AVERROR(EINVAL);
    }
  }
  ctx_->oformat = ofmt;

  if (url) {
    if (!(ctx_->url = av_strdup(url))) {
      return AVERROR(ENOMEM);
    }
  }

  if (!ctx_->pb && !(ctx_->oformat->flags & AVFMT_NOFILE)) {
    int rc = avio_open(&ctx_->pb, url, AVIO_FLAG_WRITE);
    if (rc < 0) {
      return rc;
    }
    need_close_ = true;
  }

  return 0;
}

AVStream* Muxer::new_stream() {
  return avformat_new_stream(ctx_, nullptr);
}

int Muxer::write_header(AVDictionary** opts) {
  int rc = avformat_write_header(ctx_, opts);
  if (rc >= 0) {
    need_trailer_ = true;
  }
  return rc;
}

int Muxer::write_frame(AVPacket* pkt) {
  return av_write_frame(ctx_, pkt);
}

int Muxer::interleaved_write_frame(AVPacket* pkt) {
  return av_interleaved_write_frame(ctx_, pkt);
}

void Muxer::close() {
  if (ctx_) {
    if (need_trailer_) {
      av_write_trailer(ctx_);
      need_trailer_ = false;
    }
    if (need_close_) {
      avio_closep(&ctx_->pb);
      need_close_ = false;
    }
    avformat_free_context(ctx_);
    ctx_ = nullptr;
  }
}
