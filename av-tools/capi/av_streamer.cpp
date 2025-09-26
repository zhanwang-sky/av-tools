//
//  av_streamer.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/31.
//

#include <functional>
#include <memory>
#include <stdexcept>
#include "av-tools/capi/av_streamer.h"
#include "av-tools/ffmpeg/ffmpeg_helper.hpp"
#include "av-tools/utils/rtmp_streamer.hpp"

using namespace av::ffmpeg;
using namespace av::utils;

struct AVIOHelper {
  AVIOHelper(const char* url)
      : streamer_(url)
  {
    uint8_t* io_buf = (uint8_t*) av_malloc(io_buffer_size);
    if (!io_buf) {
      goto err_exit;
    }

    avio_ = avio_alloc_context(io_buf, io_buffer_size, 1, &streamer_,
                               nullptr, url_write, nullptr);
    if (!avio_) {
      goto err_exit;
    }

    return;

  err_exit:
    av_freep(&io_buf);
    throw std::runtime_error("AVIOHelper: Cannot allocate memory");
  }

  ~AVIOHelper() {
    av_freep(&avio_->buffer);
    avio_context_free(&avio_);
  }

  inline bool connect() { return streamer_.connect(); }

  inline AVIOContext* ctx() { return avio_; }

  static int url_write(void* opaque, const uint8_t *buf, int size) {
    auto p_streamer = static_cast<RTMPStreamer*>(opaque);
    int ret = p_streamer->write(buf, size);
    if (!ret) {
      return AVERROR_EOF;
    }
    return ret;
  }

  static constexpr int io_buffer_size = 32768;
  RTMPStreamer streamer_;
  AVIOContext* avio_ = nullptr;
};

struct av_streamer {
 public:
  av_streamer(int sample_rate, int nb_channels, const char* url,
              int ar = 16000,
              int ac = 1,
              enum AVCodecID acodec = AV_CODEC_ID_AAC,
              int64_t ab = 0,
              enum AVSampleFormat sample_fmt = AV_SAMPLE_FMT_FLTP)
      : audio_frame_(av_frame_alloc(), &frame_deleter),
        audio_fifo_(av_audio_fifo_alloc(sample_fmt, ac, ar), &av_audio_fifo_free),
        resampler_(sample_rate, ChannelLayoutHelper{nb_channels}.get(), AV_SAMPLE_FMT_S16,
                   ar, ChannelLayoutHelper{ac}.get(), sample_fmt),
        audio_encode_helper_(acodec,
                             std::bind(&av_streamer::on_audio_pkt,
                                       this,
                                       std::placeholders::_1)),
        io_helper_(url)
  {
    if (!audio_frame_ || !audio_fifo_) {
      throw std::runtime_error("av_streamer: Cannot allocate memory");
    }

    auto& audio_encoder = audio_encode_helper_.encoder_;
    AVCodecContext* audio_enc_ctx = audio_encoder.ctx();
    AVFormatContext* mux_ctx = muxer_.ctx();

    // connect to rtmp server
    if (!io_helper_.connect()) {
      throw std::runtime_error("av_streamer: error connecting to rtmp server");
    }

    // setup muxer
    mux_ctx->pb = io_helper_.ctx();
    if (muxer_.open(url, "flv") < 0) {
      throw std::runtime_error("av_streamer: error opening muxer");
    }

    // setup audio encoder
    audio_enc_ctx->bit_rate = ab;
    audio_enc_ctx->time_base = av_make_q(1, ar);
    audio_enc_ctx->sample_rate = ar;
    audio_enc_ctx->sample_fmt = sample_fmt;
    av_channel_layout_default(&audio_enc_ctx->ch_layout, ac);
    if (mux_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
      audio_enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    audio_encoder.open();

    // setup audio stream
    audio_stream_ = muxer_.new_stream();
    if (!audio_stream_) {
      throw std::runtime_error("av_streamer: error creating audio_stream");
    }
    if (avcodec_parameters_from_context(audio_stream_->codecpar, audio_enc_ctx) < 0) {
      throw std::runtime_error("av_streamer: error copying audio_codecpar");
    }
    audio_stream_->time_base = av_make_q(1, 1000); // 1ms for flv

    // write flv header
    if (muxer_.write_header() < 0) {
      throw std::runtime_error("av_streamer: error writing header");
    }
  }

  ~av_streamer() = default;

  void write_audio(const uint8_t* const* data, int nb_samples) {
    if (resampler_.resample(data, nb_samples, audio_fifo_.get()) < 0) {
      throw std::runtime_error("av_streamer: error resampling audio_data");
    }

    AVCodecContext* audio_enc_ctx = audio_encode_helper_.encoder_.ctx();
    int frame_size = audio_enc_ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE ?
                     audio_enc_ctx->sample_rate : audio_enc_ctx->frame_size;

    while (av_audio_fifo_size(audio_fifo_.get()) >= frame_size) {
      if (audio_frame_->nb_samples != frame_size) {
        av_frame_unref(audio_frame_.get());
        audio_frame_->nb_samples = frame_size;
        audio_frame_->format = audio_enc_ctx->sample_fmt;
        av_channel_layout_copy(&audio_frame_->ch_layout, &audio_enc_ctx->ch_layout);
        if (av_frame_get_buffer(audio_frame_.get(), 0) < 0) {
          throw std::runtime_error("av_streamer: error getting audio_buffer");
        }
      } else {
        if (av_frame_make_writable(audio_frame_.get()) < 0) {
          throw std::runtime_error("av_streamer: error copying audio_buffer");
        }
      }

      int rc = av_audio_fifo_read(audio_fifo_.get(),
                                  reinterpret_cast<void* const*>(audio_frame_->data),
                                  frame_size);
      if (rc != frame_size) {
        throw std::runtime_error("av_streamer: error reading audio_fifo");
      }

      audio_frame_->pts = audio_pts_;
      audio_pts_ += frame_size;

      if (audio_encode_helper_.encode(audio_frame_.get()) < 0) {
        throw std::runtime_error("av_streamer: error encoding audio_frame");
      }
    }
  }

 private:
  void on_audio_pkt(AVPacket* pkt) {
    AVCodecContext* audio_enc_ctx = audio_encode_helper_.encoder_.ctx();
    av_packet_rescale_ts(pkt, audio_enc_ctx->time_base, audio_stream_->time_base);
    pkt->stream_index = audio_stream_->index;
    if (muxer_.interleaved_write_frame(pkt) < 0) {
      throw std::runtime_error("av_streamer: error writing audio_packet");
    }
  }

  std::unique_ptr<AVFrame, decltype(&frame_deleter)> audio_frame_;
  std::unique_ptr<AVAudioFifo, decltype(&av_audio_fifo_free)> audio_fifo_;
  Resampler resampler_;
  EncodeHelper audio_encode_helper_;
  AVIOHelper io_helper_;
  Muxer muxer_;
  AVStream* audio_stream_ = nullptr;
  int64_t audio_pts_ = 0;
};

av_streamer_t* av_streamer_alloc(int sample_rate, int nb_channels,
                                 const char* url) {
  try {
    return new av_streamer(sample_rate, nb_channels, url);
  } catch (...) { return nullptr; }
}

void av_streamer_free(av_streamer_t* p_streamer) {
  delete p_streamer;
}

int av_streamer_write_audio(av_streamer_t* p_streamer,
                            const unsigned char* audio_data,
                            int nb_samples) {
  try {
    const uint8_t* data[1] = {audio_data};
    p_streamer->write_audio(data, nb_samples);
    return 0;
  } catch (...) { return -1; }
}
