//
//  ffmpeg_helper.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/14.
//

#include <stdexcept>
#include <utility>
#include "ffmpeg_helper.hpp"

using namespace av::ffmpeg;

DecodeHelper::DecodeHelper(packet_callback&& pkt_cb,
                           frame_callback&& frame_cb,
                           const char* filename,
                           const AVInputFormat* ifmt,
                           AVDictionary** opts)
    : pkt_cb_(std::move(pkt_cb)),
      frame_cb_(std::move(frame_cb)),
      pkt_(av_packet_alloc(), &pkt_deleter),
      frame_(av_frame_alloc(), &frame_deleter),
      demuxer_(filename, ifmt, opts)
{
  if (!pkt_ || !frame_) {
    throw std::runtime_error("DecodeHelper: Cannot allocate memory");
  }

  AVFormatContext* demux_ctx = demuxer_.ctx();

  for (unsigned int i = 0; i != demux_ctx->nb_streams; ++i) {
    AVStream* st = demux_ctx->streams[i];
    auto& decoder = decoders_.emplace_back(st->codecpar->codec_id);
    AVCodecContext* decode_ctx = decoder.ctx();
    int rc = avcodec_parameters_to_context(decode_ctx, st->codecpar);
    if (rc < 0) {
      char err_msg[AV_ERROR_MAX_STRING_SIZE << 1];
      int msg_len = snprintf(err_msg, sizeof(err_msg), "%s", "DecodeHelper: ");
      av_strerror(rc, err_msg + msg_len, sizeof(err_msg) - msg_len);
      throw std::runtime_error(err_msg);
    }
    decode_ctx->pkt_timebase = st->time_base;
    decoder.open();
  }
}

int DecodeHelper::read() {
  int rc;

  rc = demuxer_.read_frame(pkt_.get());
  if (rc < 0) {
    if (rc == AVERROR_EOF) {
      return 0;
    }
    return rc;
  }

  do {
    auto& decoder = decoders_.at(pkt_->stream_index);

    if (!pkt_cb_(pkt_.get())) {
      rc = 1;
      break;
    }

    rc = decoder.send_packet(pkt_.get());
    if (rc < 0) {
      break;
    }

    for (;;) {
      rc = decoder.receive_frame(frame_.get());
      if (rc < 0) {
        if (rc == AVERROR(EAGAIN)) {
          rc = 1;
        }
        break;
      }
      frame_cb_(pkt_->stream_index, frame_.get());
    }

  } while (0);

  av_packet_unref(pkt_.get());

  return rc;
}

void DecodeHelper::flush(std::span<const unsigned int> ids) {
  for (auto id : ids) {
    auto& decoder = decoders_.at(id);
    int rc = decoder.send_packet(nullptr);
    if (rc < 0) {
      continue;
    }
    for (;;) {
      rc = decoder.receive_frame(frame_.get());
      if (rc < 0) {
        continue;
      }
      frame_cb_(id, frame_.get());
    }
  }
}

EncodeHelper::EncodeHelper(enum AVCodecID codec_id, packet_callback&& pkt_cb)
    : encoder_(codec_id),
      pkt_(av_packet_alloc(), &pkt_deleter),
      pkt_cb_(std::move(pkt_cb))
{
  if (!pkt_) {
    throw std::runtime_error("EncodeHelper: Cannot allocate memory");
  }
}

EncodeHelper::EncodeHelper(const char* codec_name, packet_callback&& pkt_cb)
    : encoder_(codec_name),
      pkt_(av_packet_alloc(), &pkt_deleter),
      pkt_cb_(std::move(pkt_cb))
{
  if (!pkt_) {
    throw std::runtime_error("EncodeHelper: Cannot allocate memory");
  }
}

int EncodeHelper::encode(const AVFrame* frame) {
  int rc = encoder_.send_frame(frame);
  if (rc < 0) {
    return rc;
  }

  for (;;) {
    rc = encoder_.receive_packet(pkt_.get());
    if (rc < 0) {
      if ((rc == AVERROR(EAGAIN)) || (rc == AVERROR_EOF)) {
        rc = 0;
      }
      break;
    }
    pkt_cb_(pkt_.get());
  }

  return rc;
}
