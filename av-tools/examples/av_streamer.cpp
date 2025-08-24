//
//  av_streamer.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/31.
//

#include <functional>
#include <stdexcept>
#include "ffmpeg_helper.hpp"

#include "av_streamer.h"

using namespace av::ffmpeg;

struct av_streamer {
 public:
  av_streamer(const char* url, int ar, int ac,
              enum AVCodecID acodec = AV_CODEC_ID_AAC,
              int64_t ab = 0,
              enum AVSampleFormat sample_fmt = AV_SAMPLE_FMT_FLTP)
      : muxer_(url),
        audio_encode_helper_(acodec,
                             std::bind(&av_streamer::on_audio_pkt,
                                       this,
                                       std::placeholders::_1)),
        audio_encoder_(audio_encode_helper_.encoder_),
        audio_enc_ctx_(audio_encoder_.ctx())
  {
    AVFormatContext* mux_ctx = muxer_.ctx();

    // setup audio encoder
    audio_enc_ctx_->bit_rate = ab;
    audio_enc_ctx_->time_base = av_make_q(1, ar);
    audio_enc_ctx_->sample_rate = ar;
    audio_enc_ctx_->sample_fmt = sample_fmt;
    av_channel_layout_default(&audio_enc_ctx_->ch_layout, ac);
    if (mux_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
      audio_enc_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    audio_encoder_.open();

    // setup audio stream
    audio_stream_ = muxer_.new_stream();
    if (!audio_stream_) {
      throw std::runtime_error("av_streamer: error creating audio_stream");
    }
    if (avcodec_parameters_from_context(audio_stream_->codecpar, audio_enc_ctx_) < 0) {
      throw std::runtime_error("av_streamer: error copying audio_codecpar");
    }
    audio_stream_->time_base = av_make_q(1, 1000); // 1ms for flv

    if (muxer_.write_header() < 0) {
      throw std::runtime_error("av_streamer: error writing header");
    }
  }

  ~av_streamer() = default;

 private:
  void on_audio_pkt(AVPacket* pkt) {
    av_packet_rescale_ts(pkt, audio_enc_ctx_->time_base, audio_stream_->time_base);
    pkt->stream_index = audio_stream_->index;
    if (muxer_.interleaved_write_frame(pkt) < 0) {
      throw std::runtime_error("av_streamer: error writing audio_packet");
    }
  }

  Muxer muxer_;
  EncodeHelper audio_encode_helper_;
  Encoder& audio_encoder_;
  AVCodecContext* audio_enc_ctx_;
  AVStream* audio_stream_ = nullptr;
  int64_t audio_pts_ = 0;
};

av_streamer_t* av_streamer_alloc(const char* url, int sample_rate, int nb_channels) {
  try {
    return new(std::nothrow) av_streamer(url, sample_rate, nb_channels);
  } catch (...) { return nullptr; }
}

void av_streamer_free(av_streamer_t* p_streamer) {
  delete p_streamer;
}
