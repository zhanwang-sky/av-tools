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
  using on_audio_cb = std::function<void(unsigned char*, int)>; // S16
  using on_video_cb = std::function<void(unsigned char* planes[], int strides[])>; // I420

  MediaCapture(int nb_channels, int sample_rate,
               int width, int height, int frame_rate,
               on_audio_cb&& on_audio, on_video_cb&& on_video);

  ~MediaCapture();

  void start();

  void stop();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
