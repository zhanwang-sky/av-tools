//
//  ffmpeg_helper.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/14.
//

#include <stdexcept>
#include "ffmpeg_helper.hpp"

using namespace av;

DecodeHelper::DecodeHelper(const char* filename)
    : frame_(av_frame_alloc(), frame_deleter),
      pkt_(av_packet_alloc(), pkt_deleter),
      demuxer_(filename) {
  if (!frame_ || !pkt_) {
    throw std::runtime_error("DecodeHelper: fail to alloc objects");
  }

  AVFormatContext* demux_ctx = demuxer_.ctx();

  av_dump_format(demux_ctx, 0, filename, 0);

  for (unsigned i = 0; i != demux_ctx->nb_streams; ++i) {
    AVStream* st = demux_ctx->streams[i];
    auto& decoder = decoders_.emplace_back(st->codecpar->codec_id);
    AVCodecContext* decode_ctx = decoder.ctx();
    if (avcodec_parameters_to_context(decode_ctx, st->codecpar) < 0) {
      throw std::runtime_error("DecodeHelper: fail to copy codecpar");
    }
    decode_ctx->pkt_timebase = st->time_base;
    decoder.open();
  }
}

int DecodeHelper::read(on_read_cb&& on_read) {
  int rc = 0;

  rc = demuxer_.read_frame(pkt_.get());
  if (rc == AVERROR_EOF) {
    return 0;
  } else if (rc < 0) {
    return -1;
  }

  if (on_read(pkt_->stream_index, pkt_.get(), nullptr)) {
    auto& decoder = decoders_[pkt_->stream_index];
    rc = decoder.send_packet(pkt_.get());
    while (rc == 0) {
      rc = decoder.receive_frame(frame_.get());
      if (rc < 0) {
        if (rc == AVERROR(EAGAIN)) {
          rc = 0;
        }
        break;
      }
      on_read(pkt_->stream_index, pkt_.get(), frame_.get());
    }
  }

  av_packet_unref(pkt_.get());

  return (rc == 0) ? 1 : -1;
}

int DecodeHelper::flush(unsigned stream_id, on_read_cb&& on_read) {
  auto& decoder = decoders_[stream_id];
  int rc = 0;

  rc = decoder.send_packet(nullptr);
  while (rc == 0) {
    rc = decoder.receive_frame(frame_.get());
    if (rc < 0) {
      if (rc == AVERROR_EOF) {
        rc = 0;
      }
      break;
    }
    on_read(stream_id, nullptr, frame_.get());
  }

  return (rc == 0) ? 0 : -1;
}
