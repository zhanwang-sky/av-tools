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
  using on_audio_cb = std::function<void(const unsigned char*, int)>;

  MediaCapture(int sample_rate, int nb_channels, on_audio_cb&& on_audio);

  ~MediaCapture();

  void start();

  void stop();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
