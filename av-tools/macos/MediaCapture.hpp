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
  using audio_callback = std::function<void(unsigned char*, int)>; // S16
  using video_callback = std::function<void(unsigned char* planes[], int strides[])>; // I420

  MediaCapture(int nb_channels, int sample_rate,
               int width, int height, int frame_rate,
               audio_callback&& audio_cb, video_callback&& video_cb);

  ~MediaCapture();

  void start();

  void stop();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
