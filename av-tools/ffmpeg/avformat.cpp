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

AVDemuxer::AVDemuxer() { }

AVDemuxer::AVDemuxer(const char* url,
                     const AVInputFormat* fmt,
                     AVDictionary** opts) {
  open(url, fmt, opts);
}

AVDemuxer::~AVDemuxer() {
  close();
}

void AVDemuxer::open(const char* url,
                     const AVInputFormat* fmt,
                     AVDictionary** opts) {
  char err_msg[AV_ERROR_MAX_STRING_SIZE];
  int rc;

  assert(!ctx_);

  rc = avformat_open_input(&ctx_, url, fmt, opts);
  if (rc < 0) {
    goto err_exit;
  }

  rc = avformat_find_stream_info(ctx_, nullptr);
  if (rc < 0) {
    goto err_exit;
  }

  return;

err_exit:
  av_strerror(rc, err_msg, sizeof(err_msg));
  close();
  throw std::runtime_error(err_msg);
}

void AVDemuxer::close() {
  avformat_close_input(&ctx_);
}

int AVDemuxer::read_frame(AVPacket* pkt) {
  assert(ctx_);
  return av_read_frame(ctx_, pkt);
}

AVMuxer::AVMuxer() { }

AVMuxer::AVMuxer(const char* url,
                 const char* fmt_name,
                 const AVOutputFormat* fmt) {
  open(url, fmt_name, fmt);
}

AVMuxer::~AVMuxer() {
  close();
}

void AVMuxer::open(const char* url,
                   const char* fmt_name,
                   const AVOutputFormat* fmt) {
  char err_msg[AV_ERROR_MAX_STRING_SIZE];
  int rc;

  assert(!ctx_);

  rc = avformat_alloc_output_context2(&ctx_, fmt, fmt_name, url);
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

void AVMuxer::close() {
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

AVStream* AVMuxer::new_stream() {
  assert(ctx_);
  return avformat_new_stream(ctx_, nullptr);
}

int AVMuxer::write_header(AVDictionary** opts) {
  int rc;

  assert(ctx_);

  rc = avformat_write_header(ctx_, opts);
  if (rc >= 0) {
    need_trailer_ = true;
  }

  return rc;
}

int AVMuxer::write_frame(AVPacket* pkt) {
  assert(ctx_);
  return av_write_frame(ctx_, pkt);
}

int AVMuxer::interleaved_write_frame(AVPacket* pkt) {
  assert(ctx_);
  return av_interleaved_write_frame(ctx_, pkt);
}
