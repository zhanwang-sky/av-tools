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
      pkt_(av_packet_alloc(), pkt_deleter),
      frame_(av_frame_alloc(), frame_deleter),
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

std::unique_ptr<EncodeHelper>
EncodeHelper::FromCodecID(const char* filename,
                          const std::vector<AVCodecID>& vid,
                          on_stream_cb&& on_stream) noexcept(false) {
  auto p_helper = std::make_unique<EncodeHelper>(filename);
  auto& muxer = p_helper->muxer_;

  for (std::size_t i = 0; i != vid.size(); ++i) {
    auto& encoder = p_helper->encoders_.emplace_back(vid[i]);
    AVStream* st = muxer.new_stream();
    if (!st) {
      throw std::runtime_error("FromCodecID: fail to create output stream");
    }
    on_stream(muxer, encoder, st, static_cast<unsigned>(i));
  }

  return p_helper;
}

EncodeHelper::EncodeHelper(const char* filename)
    : pkt_(av_packet_alloc(), pkt_deleter),
      muxer_(filename) {
  if (!pkt_) {
    throw std::runtime_error("EncodeHelper: fail to alloc objects");
  }
}

int EncodeHelper::write(unsigned stream_id,
                        AVFrame* frame,
                        on_write_cb&& on_write) {
  auto& encoder = encoders_[stream_id];
  int rc = 0;
  int pkt_cnt = 0;

  rc = encoder.send_frame(frame);
  while (rc == 0) {
    rc = encoder.receive_packet(pkt_.get());
    if (rc < 0) {
      if ((rc == AVERROR(EAGAIN)) || (rc == AVERROR_EOF)) {
        rc = 0;
      }
      break;
    }
    ++pkt_cnt;
    if (on_write(stream_id, frame, pkt_.get())) {
      rc = muxer_.interleaved_write_frame(pkt_.get());
    }
  }

  return (rc == 0) ? pkt_cnt : -1;
}
