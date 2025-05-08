//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include "av_streamer.h"
#include "MediaCapture.hpp"

#define NB_CHANNELS 2
#define SAMPLE_RATE 44100
#define VIDEO_WIDTH 1280
#define VIDEO_HEIGHT 720
#define FRAME_RATE 30

using std::cout;
using std::cerr;
using std::endl;

class MyApp {
 public:
  MyApp(const char* url)
      : streamer_(av_streamer_alloc(url, NB_CHANNELS, SAMPLE_RATE),
                  &av_streamer_free),
        media_capture_(NB_CHANNELS, SAMPLE_RATE,
                       VIDEO_WIDTH, VIDEO_HEIGHT, FRAME_RATE,
                       std::bind(&MyApp::on_audio, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
                       std::bind(&MyApp::on_video, this,
                                 std::placeholders::_1)) { }

  virtual ~MyApp() = default;

  inline void start() { media_capture_.start(); }

  inline void stop() { media_capture_.stop(); }

 private:
  void on_audio(const unsigned char* data, int samples) {
    av_streamer_write_samples(streamer_.get(), data, samples);
  }

  void on_video(const MediaCapture::Frame& frame) {
    // XXX TODO
  }

  std::unique_ptr<av_streamer_t, decltype(&av_streamer_free)> streamer_;
  MediaCapture media_capture_;
};

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cerr << "Usage: ./av-tools <url>\n";
    exit(EXIT_FAILURE);
  }

  try {
    MyApp app(argv[1]);

    app.start();

    cout << "press Enter to exit...\n";
    std::cin.get();

    app.stop();

  } catch (const std::exception& e) {
    cerr << "Exception caught: " << e.what() << endl;
    exit(EXIT_FAILURE);
  }

  return 0;
}
