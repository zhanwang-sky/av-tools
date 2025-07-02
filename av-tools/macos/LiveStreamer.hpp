//
//  LiveStreamer.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/7/2.
//

#pragma once

#include <fstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

#include <libyuv.h>

#include "av_streamer.h"
#include "MediaCapture.hpp"

#define NB_CHANNELS 2
#define SAMPLE_RATE 44100
#define VIDEO_WIDTH 1280
#define VIDEO_HEIGHT 720
#define FRAME_RATE 30

class LiveStreamer {
 public:
  LiveStreamer(const char* url, const char* ppm_path)
      : streamer_(av_streamer_alloc(url, NB_CHANNELS, SAMPLE_RATE),
                  &av_streamer_free),
        media_capture_(NB_CHANNELS, SAMPLE_RATE,
                       VIDEO_WIDTH, VIDEO_HEIGHT, FRAME_RATE,
                       std::bind(&LiveStreamer::on_audio, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
                       std::bind(&LiveStreamer::on_video, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2)),
        ppm_(ppm_path, std::fstream::binary | std::fstream::trunc)
  {
    if (!ppm_) {
      throw std::runtime_error("fail to open ppm file for writing");
    }
  }

  virtual ~LiveStreamer() = default;

  inline void start() { media_capture_.start(); }

  inline void stop() { media_capture_.stop(); }

 private:
  void on_audio(unsigned char* data, int samples) {
    av_streamer_write_samples(streamer_.get(), data, samples);
  }

  void on_video(unsigned char* planes[], int strides[]) {
    if (ppm_.is_open()) {
      if (++frame_cnt_ == 10) {
        std::vector<uint8_t> rgb24_buf(VIDEO_WIDTH * VIDEO_HEIGHT * 3);
        libyuv::I420ToRGB24Matrix(planes[0], strides[0],
                                  planes[2], strides[2],
                                  planes[1], strides[1],
                                  rgb24_buf.data(), VIDEO_WIDTH * 3,
                                  &libyuv::kYuvH709ConstantsVU,
                                  VIDEO_WIDTH, VIDEO_HEIGHT);
        ppm_ << "P6\n" << VIDEO_WIDTH << " " << VIDEO_HEIGHT << "\n255\n";
        ppm_.write((const char*) rgb24_buf.data(), rgb24_buf.size());
        ppm_.close();
      }
    }
  }

  std::unique_ptr<av_streamer_t, decltype(&av_streamer_free)> streamer_;
  MediaCapture media_capture_;
  std::ofstream ppm_;
  int frame_cnt_ = 0;
};
