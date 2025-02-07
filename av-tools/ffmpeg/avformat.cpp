//
//  avformat.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cassert>
#include <stdexcept>

#include "avformat.hpp"

using namespace av::ffmpeg;

AVOFormat::AVOFormat() { }

AVOFormat::AVOFormat(const char* url, const char* format) {
  open(url, format);
}

AVOFormat::~AVOFormat() {
  close();
}

void AVOFormat::open(const char* url, const char* format) {
  char err_msg[AV_ERROR_MAX_STRING_SIZE];
  int rc;

  assert(!ctx_);

  rc = avformat_alloc_output_context2(&ctx_, NULL, format, url);
  if (rc < 0) {
    goto err_exit;
  }

  if (!(ctx_->oformat->flags & AVFMT_NOFILE)) {
    rc = avio_open(&ctx_->pb, url, AVIO_FLAG_WRITE);
    if (rc < 0) {
      goto err_exit;
    }
    need_close_ = true;
  }

  return;

err_exit:
  av_strerror(rc, err_msg, sizeof(err_msg));
  close();
  throw std::runtime_error(err_msg);
}

void AVOFormat::close() {
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

AVStream* AVOFormat::new_stream() {
  assert(ctx_);
  return avformat_new_stream(ctx_, nullptr);
}

int AVOFormat::write_header(AVDictionary** opts) {
  int rc;

  assert(ctx_);

  rc = avformat_write_header(ctx_, opts);
  if (rc >= 0) {
    need_trailer_ = true;
  }

  return rc;
}

int AVOFormat::write_frame(AVPacket* pkt) {
  assert(ctx_);
  return av_write_frame(ctx_, pkt);
}

int AVOFormat::interleaved_write_frame(AVPacket* pkt) {
  assert(ctx_);
  return av_interleaved_write_frame(ctx_, pkt);
}
