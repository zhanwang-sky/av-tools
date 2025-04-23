//
//  MediaCapture.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/4/16.
//

#pragma once

#include <functional>
#include <memory>

class MediaCapture final {
 public:
  struct Frame {
    int width;
    int height;
    unsigned char* planes[3];
    int strides[3];
  };

  using on_audio_cb = std::function<void(const unsigned char*, int)>;
  using on_video_cb = std::function<void(const Frame&)>;

  MediaCapture(int nb_channels, int sample_rate, on_audio_cb&& on_audio,
               on_video_cb&& on_video);

  ~MediaCapture();

  void start();

  void stop();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
