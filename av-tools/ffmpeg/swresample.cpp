//
//  swresample.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/5.
//

#include <stdexcept>
#include <string>

#include "swresample.hpp"

using namespace av::ffmpeg;

Resampler::Resampler(int in_sample_rate, const AVChannelLayout& in_ch_layout, enum AVSampleFormat in_sample_fmt,
                     int out_sample_rate, const AVChannelLayout& out_ch_layout, enum AVSampleFormat out_sample_fmt)
    : in_sample_rate_(in_sample_rate),
      out_sample_rate_(out_sample_rate),
      in_sample_fmt_(in_sample_fmt),
      out_sample_fmt_(out_sample_fmt)
{
  int rc;
  char err_msg[AV_ERROR_MAX_STRING_SIZE];
  std::string err_str;

  rc = av_channel_layout_copy(&in_ch_layout_, &in_ch_layout);
  if (rc < 0) {
    goto err_exit;
  }

  rc = av_channel_layout_copy(&out_ch_layout_, &out_ch_layout);
  if (rc < 0) {
    goto err_exit;
  }

  rc = swr_alloc_set_opts2(&swr_,
                           &out_ch_layout, out_sample_fmt, out_sample_rate,
                           &in_ch_layout, in_sample_fmt, in_sample_rate,
                           0, nullptr);
  if (rc < 0) {
    goto err_exit;
  }

  rc = swr_init(swr_);
  if (rc < 0) {
    goto err_exit;
  }

  return;

err_exit:
  clean();
  av_strerror(rc, err_msg, sizeof(err_msg));
  err_str.append("Resampler: ").append(err_msg);
  throw std::runtime_error(err_str);
}

Resampler::Resampler(Resampler&& rhs) noexcept
    : in_sample_rate_(rhs.in_sample_rate_),
      out_sample_rate_(rhs.out_sample_rate_),
      in_sample_fmt_(rhs.in_sample_fmt_),
      out_sample_fmt_(rhs.out_sample_fmt_),
      in_ch_layout_(rhs.in_ch_layout_),
      out_ch_layout_(rhs.out_ch_layout_),
      swr_(rhs.swr_),
      samples_buf_(rhs.samples_buf_),
      samples_(rhs.samples_)
{
  rhs.reset();
}

Resampler& Resampler::operator=(Resampler&& rhs) noexcept {
  if (this != &rhs) {
    clean();

    in_sample_rate_ = rhs.in_sample_rate_;
    out_sample_rate_ = rhs.out_sample_rate_;
    in_sample_fmt_ = rhs.in_sample_fmt_;
    out_sample_fmt_ = rhs.out_sample_fmt_;
    in_ch_layout_ = rhs.in_ch_layout_;
    out_ch_layout_ = rhs.out_ch_layout_;
    swr_ = rhs.swr_;
    samples_buf_ = rhs.samples_buf_;
    samples_ = rhs.samples_;

    rhs.reset();
  }

  return *this;
}

Resampler::~Resampler() {
  clean();
}

int Resampler::resample(const uint8_t* const* in_samples_buf, int in_samples, AVAudioFifo* af) {
  int out_samples;
  int rc;

  out_samples = swr_get_out_samples(swr_, in_samples);
  if (out_samples < 0) {
    return out_samples;
  }

  out_samples = FFMAX(out_samples, out_sample_rate_);

  // realloc
  if (samples_ < out_samples) {
    if (samples_buf_) {
      av_freep(&samples_buf_[0]);
    }
  }

  if (!samples_buf_ || !samples_buf_[0]) {
    if (!samples_buf_) {
      // first alloc
      rc = av_samples_alloc_array_and_samples(&samples_buf_, nullptr,
                                              out_ch_layout_.nb_channels,
                                              out_samples, out_sample_fmt_,
                                              0);
    } else {
      // realloc
      rc = av_samples_alloc(&samples_buf_[0], nullptr,
                            out_ch_layout_.nb_channels,
                            out_samples, out_sample_fmt_,
                            0);
    }
    if (rc < 0) {
      return rc;
    }
    samples_ = out_samples;
  }

  out_samples = swr_convert(swr_,
                            samples_buf_, samples_,
                            in_samples_buf, in_samples);
  if (out_samples < 0) {
    return out_samples;
  }

  if (out_samples > 0) {
    rc = av_audio_fifo_write(af,
                             reinterpret_cast<void* const*>(samples_buf_),
                             out_samples);
    if (rc < 0) {
      return rc;
    }
  }

  return out_samples;
}

void Resampler::clean() {
  samples_ = 0;
  if (samples_buf_) {
    av_freep(&samples_buf_[0]);
  }
  av_freep(&samples_buf_);
  swr_free(&swr_);
  av_channel_layout_uninit(&out_ch_layout_);
  av_channel_layout_uninit(&in_ch_layout_);
}

void Resampler::reset() {
  in_ch_layout_ = AV_CHANNEL_LAYOUT_MONO;
  out_ch_layout_ = AV_CHANNEL_LAYOUT_MONO;
  swr_ = nullptr;
  samples_buf_ = nullptr;
  samples_ = 0;
}
