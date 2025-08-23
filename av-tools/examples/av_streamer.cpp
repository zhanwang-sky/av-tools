//
//  av_streamer.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/31.
//

#include "swresample.hpp"
#include "ffmpeg_helper.hpp"
#include "av_streamer.h"

// XXX TODO: make configurable
#define AAC_BITRATE (24 << 10)

using namespace av::ffmpeg;

#if 0

struct av_streamer {
  struct ChLayoutHelper {
    ChLayoutHelper(int nb_channels) {
      av_channel_layout_default(&layout, nb_channels);
    }
    AVChannelLayout layout;
  };

  static constexpr auto frame_deleter = [](AVFrame* frame) { av_frame_free(&frame); };
  std::unique_ptr<AVFrame, decltype(frame_deleter)> audio_frame;
  std::unique_ptr<AVAudioFifo, decltype(&av_audio_fifo_free)> audio_fifo;
  std::unique_ptr<ChLayoutHelper> layout_helper;
  std::unique_ptr<Resampler> resampler;
  std::unique_ptr<EncodeHelper> encode_helper;
  // convince vars
  Muxer& muxer;
  Encoder& audio_encoder;
  AVStream* audio_stream;
  // saved input args
  const int nb_channels;
  const int sample_rate;
  // realtime data
  int64_t nb_samples = 0;

  // pcm_s16le => flv aac
  av_streamer(const char* url, int nb_channels, int sample_rate, int64_t bit_rate)
      : audio_frame(av_frame_alloc(), frame_deleter),
        audio_fifo(av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, nb_channels, sample_rate),
                   &av_audio_fifo_free),
        layout_helper(std::make_unique<ChLayoutHelper>(nb_channels)),
        resampler(std::make_unique<Resampler>(layout_helper->layout, AV_SAMPLE_FMT_S16, sample_rate,
                                              layout_helper->layout, AV_SAMPLE_FMT_FLTP, sample_rate)),
        encode_helper(EncodeHelper::FromCodecID(
            url,
            {AV_CODEC_ID_AAC},
            [nb_channels, sample_rate, bit_rate](Muxer& muxer, Encoder& encoder, AVStream* st, unsigned st_id) {
              if (st_id == 0) {
                on_audio_stream(muxer, encoder, st, nb_channels, sample_rate, bit_rate);
              }
            })),
        muxer(encode_helper->muxer_),
        audio_encoder(encode_helper->encoders_.at(0)),
        audio_stream(muxer.ctx()->streams[0]),
        nb_channels(nb_channels),
        sample_rate(sample_rate) {
    if (!audio_frame || !audio_fifo) {
      throw std::runtime_error("av_streamer: fail to alloc objects");
    }
    if (muxer.write_header() < 0) {
      throw std::runtime_error("av_streamer: fail to write header");
    }
  }

 private:
  static void on_audio_stream(Muxer& muxer,
                              Encoder& encoder,
                              AVStream* st,
                              int nb_channels,
                              int sample_rate,
                              int64_t bit_rate) {
    AVFormatContext* mux_ctx = muxer.ctx();
    AVCodecContext* encode_ctx = encoder.ctx();

    encode_ctx->bit_rate = bit_rate;
    encode_ctx->time_base = av_make_q(1, sample_rate);
    encode_ctx->sample_rate = sample_rate;
    encode_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    av_channel_layout_default(&encode_ctx->ch_layout, nb_channels);
    if (mux_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
      encode_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    encoder.open();

    if (avcodec_parameters_from_context(st->codecpar, encode_ctx) < 0) {
      throw std::runtime_error("av_streamer::on_audio_stream fail to copy codecpar from encoder");
    }
    st->time_base = av_make_q(1, 1000); // 1ms for flv
  }
};

av_streamer_t* av_streamer_alloc(const char* url, int nb_channels, int sample_rate) {
  try {
    return new av_streamer(url, nb_channels, sample_rate, AAC_BITRATE);
  } catch (...) {
    return nullptr;
  }
}

void av_streamer_free(av_streamer_t* p_streamer) {
  delete p_streamer;
}

int av_streamer_write_samples(av_streamer_t* p_streamer,
                              const unsigned char* audio_data,
                              int nb_samples) {
  auto& encode_helper = p_streamer->encode_helper;
  auto& resampler = p_streamer->resampler;
  auto& fifo = p_streamer->audio_fifo;
  auto& frame = p_streamer->audio_frame;
  AVCodecContext* encode_ctx = p_streamer->audio_encoder.ctx();
  const uint8_t* samples_buf[1] = {audio_data};
  int rc = 0;

  // s16le => fltp
  rc = resampler->resample(samples_buf, nb_samples, fifo.get());
  if (rc < 0) {
    return -1;
  }

  auto on_write = [p_streamer](unsigned index, AVFrame* frame, AVPacket* pkt) {
    if (pkt) {
      pkt->dts = pkt->pts = av_rescale_q(p_streamer->nb_samples,
                                         p_streamer->audio_encoder.ctx()->time_base,
                                         p_streamer->audio_stream->time_base);
      pkt->duration = av_rescale_q(frame->nb_samples,
                                   p_streamer->audio_encoder.ctx()->time_base,
                                   p_streamer->audio_stream->time_base);
      pkt->pos = -1;
      pkt->stream_index = index;

      if (frame) {
        p_streamer->nb_samples += frame->nb_samples;
      }
    }
    return true;
  };

  while ((rc = av_audio_fifo_size(fifo.get())) > 0) {
    int samples2rd = (encode_ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) ?
                     rc : encode_ctx->frame_size;

    if (rc < samples2rd) {
      // not enough
      return 0;
    }

    // realloc sample buffer
    av_frame_unref(frame.get());

    frame->nb_samples = samples2rd;
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->sample_rate = p_streamer->sample_rate;
    av_channel_layout_default(&frame->ch_layout, p_streamer->nb_channels);

    if (av_frame_get_buffer(frame.get(), 0) < 0) {
      return -1;
    }

    // read from fifo
    rc = av_audio_fifo_read(fifo.get(), reinterpret_cast<void* const*>(frame->data), samples2rd);
    if (rc != samples2rd) {
      return -1;
    }

    // encode & write(send)
    rc = encode_helper->write(0, frame.get(), on_write);
    if (rc < 0) {
      return -1;
    }
  }

  return 0;
}

#endif
