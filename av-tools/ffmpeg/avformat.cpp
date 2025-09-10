//
//  avformat.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <stdexcept>
#include <string>
#include "av-tools/ffmpeg/avformat.hpp"

using namespace av::ffmpeg;

Demuxer::Demuxer(const char* url,
                 const AVInputFormat* ifmt,
                 AVDictionary** opts) {
  int rc;
  char err_msg[AV_ERROR_MAX_STRING_SIZE];
  std::string err_str;

  rc = avformat_open_input(&ctx_, url, ifmt, opts);
  if (rc < 0) {
    goto err_exit;
  }

  rc = avformat_find_stream_info(ctx_, nullptr);
  if (rc < 0) {
    goto err_exit;
  }

  return;

err_exit:
  clean();
  av_strerror(rc, err_msg, sizeof(err_msg));
  err_str.append("Demuxer: ").append(err_msg);
  throw std::runtime_error(err_str);
}

Demuxer::Demuxer(Demuxer&& rhs) noexcept
    : ctx_(rhs.ctx_)
{
  rhs.ctx_ = nullptr;
}

Demuxer& Demuxer::operator=(Demuxer&& rhs) noexcept {
  if (this != &rhs) {
    clean();
    ctx_ = rhs.ctx_;
    rhs.ctx_ = nullptr;
  }
  return *this;
}

Demuxer::~Demuxer() {
  clean();
}

int Demuxer::read_frame(AVPacket* pkt) {
  return av_read_frame(ctx_, pkt);
}

void Demuxer::clean() {
  avformat_close_input(&ctx_);
}

Muxer::Muxer(const char* url,
             const char* fmt_name,
             const AVOutputFormat* ofmt) {
  int rc;
  char err_msg[AV_ERROR_MAX_STRING_SIZE];
  std::string err_str;

  rc = avformat_alloc_output_context2(&ctx_, ofmt, fmt_name, url);
  if (rc < 0) {
    goto err_exit;
  }

  return;

err_exit:
  clean();
  av_strerror(rc, err_msg, sizeof(err_msg));
  err_str.append("Muxer: ").append(err_msg);
  throw std::runtime_error(err_str);
}

Muxer::Muxer(Muxer&& rhs) noexcept
    : ctx_(rhs.ctx_),
      need_close_(rhs.need_close_),
      need_trailer_(rhs.need_trailer_)
{
  rhs.ctx_ = nullptr;
}

Muxer& Muxer::operator=(Muxer&& rhs) noexcept {
  if (this != &rhs) {
    clean();

    ctx_ = rhs.ctx_;
    need_close_ = rhs.need_close_;
    need_trailer_ = rhs.need_trailer_;

    rhs.ctx_ = nullptr;
  }

  return *this;
}

Muxer::~Muxer() {
  clean();
}

AVStream* Muxer::new_stream() {
  return avformat_new_stream(ctx_, nullptr);
}

int Muxer::write_header(AVDictionary** opts) {
  int rc;

  if (!ctx_->pb && !(ctx_->oformat->flags & AVFMT_NOFILE)) {
    rc = avio_open(&ctx_->pb, ctx_->url, AVIO_FLAG_WRITE);
    if (rc < 0) {
      goto err_exit;
    }
    need_close_ = true;
  }

  rc = avformat_write_header(ctx_, opts);
  if (rc < 0) {
    goto err_exit;
  }
  need_trailer_ = true;

err_exit:
  return rc;
}

int Muxer::write_frame(AVPacket* pkt) {
  return av_write_frame(ctx_, pkt);
}

int Muxer::interleaved_write_frame(AVPacket* pkt) {
  return av_interleaved_write_frame(ctx_, pkt);
}

void Muxer::clean() {
  if (ctx_) {
    if (need_trailer_) {
      av_write_trailer(ctx_);
    }
    if (need_close_) {
      avio_closep(&ctx_->pb);
    }
    avformat_free_context(ctx_);
    ctx_ = nullptr;
  }
}
