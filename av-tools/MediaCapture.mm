//
//  MediaCapture.mm
//  av-tools
//
//  Created by zhanwang-sky on 2025/4/16.
//

#import <AVFoundation/AVFoundation.h>
#import <stdexcept>
#import "MediaCapture.hpp"

@interface MediaCaptureImpl : NSObject <AVCaptureAudioDataOutputSampleBufferDelegate>

- (instancetype)initWithSampleRate:(int) sampleRate
                    withChannelNum:(int) channelNum
                       withAudioCb:(MediaCapture::on_audio_cb &&) audioCb;

- (void)start;

- (void)stop;

@end

@implementation MediaCaptureImpl {
  AVCaptureSession *_session;
  AVCaptureAudioDataOutput *_audioOutput;
  dispatch_queue_t _audioDispatchQueue;
  MediaCapture::on_audio_cb _audioCb;
}

- (instancetype)initWithSampleRate:(int) sampleRate
                    withChannelNum:(int) channelNum
                       withAudioCb:(MediaCapture::on_audio_cb &&) audioCb {
  self = [super init];

  if (self) {
    NSError *err;

    _session = [[AVCaptureSession alloc] init];
    _audioOutput = [[AVCaptureAudioDataOutput alloc] init];
    _audioDispatchQueue = dispatch_queue_create(nil, DISPATCH_QUEUE_SERIAL);

    _audioCb = std::move(audioCb);

    AVCaptureDevice *audioDevice = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeAudio];
    if (!audioDevice) {
      NSLog(@"No available audio device");
      return nil;
    }

    AVCaptureDeviceInput *audioInput = [AVCaptureDeviceInput deviceInputWithDevice:audioDevice error:&err];
    if (!audioInput) {
      NSLog(@"Fail to create audio input: %@", err.localizedDescription);
      return nil;
    }

    NSDictionary *audioSettings = @{
      AVFormatIDKey: @(kAudioFormatLinearPCM),
      AVSampleRateKey: @(sampleRate),
      AVNumberOfChannelsKey: @(channelNum),
      AVLinearPCMBitDepthKey: @(16),
      AVLinearPCMIsBigEndianKey: @(NO),
      AVLinearPCMIsFloatKey: @(NO),
      AVLinearPCMIsNonInterleaved: @(NO),
    };
    [_audioOutput setAudioSettings:audioSettings];

    [_audioOutput setSampleBufferDelegate:self queue:_audioDispatchQueue];

    [_session beginConfiguration];

    if (![_session canAddInput:audioInput]) {
      NSLog(@"Cannot add audio input");
      return nil;
    }
    [_session addInput:audioInput];

    if (![_session canAddOutput:_audioOutput]) {
      NSLog(@"Cannot add audio output");
      return nil;
    }
    [_session addOutput:_audioOutput];

    [_session commitConfiguration];
  }

  return self;
}

- (void)start {
  [_session startRunning];
}

- (void)stop {
  [_session stopRunning];
}

- (void)captureOutput:(AVCaptureOutput *)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection {
  if (output == _audioOutput) {
    CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
    if (blockBuffer) {
      char *data;
      if (CMBlockBufferGetDataPointer(blockBuffer, 0, NULL, NULL, &data) == kCMBlockBufferNoErr) {
        CMItemCount samples = CMSampleBufferGetNumSamples(sampleBuffer);
        _audioCb((const unsigned char*) data, (int) samples);
      }
    }
  }
}

@end

struct MediaCapture::Impl final {
  __strong MediaCaptureImpl* instance;

  Impl(int sample_rate, int nb_channels, on_audio_cb&& on_audio)
      : instance([[MediaCaptureImpl alloc] initWithSampleRate:sample_rate
                                               withChannelNum:nb_channels
                                                  withAudioCb:(std::move(on_audio))]) {
    if (!instance) {
      throw std::runtime_error("MediaCapture::Impl: fail to init MediaCaptureImpl");
    }
  }

  ~Impl() = default;
};

MediaCapture::MediaCapture(int sample_rate, int nb_channels, on_audio_cb&& on_audio)
    : impl_(std::make_unique<Impl>(sample_rate, nb_channels, std::move(on_audio))) { }

MediaCapture::~MediaCapture() = default;

void MediaCapture::start() {
  [impl_->instance start];
}

void MediaCapture::stop() {
  [impl_->instance stop];
}
