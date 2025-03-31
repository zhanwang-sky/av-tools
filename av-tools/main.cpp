//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <AudioToolbox/AudioToolbox.h>
#include "av_streamer.h"

#define SAMPLE_RATE 8000

using std::cout;
using std::cerr;
using std::endl;

struct MyApp {
  std::unique_ptr<av_streamer_t, decltype(&av_streamer_free)> streamer;
  // saved input args
  const char* const url;

  MyApp(const char* url)
      : streamer(av_streamer_alloc(url, SAMPLE_RATE), &av_streamer_free),
        url(url) {
    if (!streamer) {
      throw std::runtime_error("MyApp: fail to alloc av_streamer");
    }
  }

  static void onAudio(void* inUserData,
                      AudioQueueRef inAQ,
                      AudioQueueBufferRef inBuffer,
                      const AudioTimeStamp* inStartTime,
                      UInt32 inNumberPacketDescriptions,
                      const AudioStreamPacketDescription* inPacketDescs);
};

void MyApp::onAudio(void* inUserData,
                    AudioQueueRef inAQ,
                    AudioQueueBufferRef inBuffer,
                    const AudioTimeStamp* inStartTime,
                    UInt32 inNumberPacketDescriptions,
                    const AudioStreamPacketDescription* inPacketDescs) {
  MyApp* this_ = static_cast<MyApp*>(inUserData);

  int rc = av_streamer_write_samples(this_->streamer.get(),
                                     static_cast<const unsigned char*>(inBuffer->mAudioData),
                                     inBuffer->mAudioDataByteSize >> 1);
  if (rc < 0) {
    cerr << "error writing samples, rc=" << rc << endl;
  }

  AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cerr << "Usage: ./av-tools <rtmp://balabala.com/foo/bar>\n";
    exit(EXIT_FAILURE);
  }

  try {
    MyApp myApp(argv[1]);

    AudioStreamBasicDescription format = {0};
    format.mSampleRate = SAMPLE_RATE;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    format.mBitsPerChannel = sizeof(SInt16) << 3;
    format.mChannelsPerFrame = 1;
    format.mBytesPerFrame = (format.mBitsPerChannel >> 3) * format.mChannelsPerFrame;
    format.mFramesPerPacket = 1;
    format.mBytesPerPacket = format.mBytesPerFrame * format.mFramesPerPacket;

    AudioQueueRef queue = {0};
    if (AudioQueueNewInput(&format,
                           myApp.onAudio,
                           &myApp,
                           nullptr,
                           nullptr,
                           0,
                           &queue) != noErr) {
      throw std::runtime_error("Fail to create AudioQueue");
    }

    AudioQueueBufferRef buffer = {0};
    UInt32 bufferSize = format.mSampleRate * format.mBytesPerFrame * 20 / 1000; // 20ms
    if (AudioQueueAllocateBuffer(queue, bufferSize, &buffer) != noErr) {
      throw std::runtime_error("Fail to alloc AudioQueueBuffer");
    }

    if (AudioQueueEnqueueBuffer(queue, buffer, 0, NULL) != noErr) {
      throw std::runtime_error("Fail to EnqueueBuffer");
    }

    if (AudioQueueStart(queue, NULL) != noErr) {
      throw std::runtime_error("Fail to start AudioQueue");
    }

    cout << "Press any key to exit...\n";
    std::cin.get();

    if (AudioQueueStop(queue, true) != noErr) {
      throw std::runtime_error("Fail to stop AudioQueue");
    }

    if (AudioQueueDispose(queue, true) != noErr) {
      throw std::runtime_error("Fail to dispose AudioQueue");
    }

  } catch (const std::exception& e) {
    cerr << "Exception caught: " << e.what() << endl;
    exit(EXIT_FAILURE);
  }

  return 0;
}
