//
//  swresample.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/5.
//

#pragma once

extern "C" {
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
}

namespace av {

namespace ffmpeg {

class Resampler {
 public:
  Resampler(const Resampler&) = delete;
  Resampler& operator=(const Resampler&) = delete;

  explicit Resampler(int in_sample_rate, const AVChannelLayout& in_ch_layout, enum AVSampleFormat in_sample_fmt,
                     int out_sample_rate, const AVChannelLayout& out_ch_layout, enum AVSampleFormat out_sample_fmt);

  Resampler(Resampler&& rhs) noexcept;

  Resampler& operator=(Resampler&& rhs) noexcept;

  virtual ~Resampler();

  int resample(const uint8_t* const* in_samples_buf, int in_samples, AVAudioFifo* af);

 protected:
  virtual void clean();
  virtual void reset();

 private:
  int in_sample_rate_;
  int out_sample_rate_;
  enum AVSampleFormat in_sample_fmt_;
  enum AVSampleFormat out_sample_fmt_;
  AVChannelLayout in_ch_layout_{};
  AVChannelLayout out_ch_layout_{};
  struct SwrContext* swr_ = nullptr;
  uint8_t** samples_buf_ = nullptr;
  int samples_ = 0;
};

} // ffmpeg

} // av
