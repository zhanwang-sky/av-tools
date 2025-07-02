//
//  MediaCapture.mm
//  av-tools
//
//  Created by zhanwang-sky on 2025/4/16.
//

#import <AVFoundation/AVFoundation.h>
#import <stdexcept>
#import <utility>
#import "MediaCapture.hpp"

@interface MediaCaptureImpl : NSObject <AVCaptureAudioDataOutputSampleBufferDelegate,
                                        AVCaptureVideoDataOutputSampleBufferDelegate>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithChannelNum:(int)channelNum
                    withSampleRate:(int)sampleRate
                    withVideoWidth:(int)videoWidth
                   withVideoHeight:(int)videoHeight
                     withFrameRate:(int)frameRate
                       withAudioCb:(MediaCapture::on_audio_cb &&)audioCb
                       withVideoCb:(MediaCapture::on_video_cb &&)videoCb;

- (void)start;

- (void)stop;

@end

@implementation MediaCaptureImpl {
  AVCaptureSession *_session;
  AVCaptureAudioDataOutput *_audioOutput;
  AVCaptureVideoDataOutput *_videoOutput;
  dispatch_queue_t _audioDispatchQueue;
  dispatch_queue_t _videoDispatchQueue;
  MediaCapture::on_audio_cb _audioCb;
  MediaCapture::on_video_cb _videoCb;
}

- (instancetype)initWithChannelNum:(int)channelNum
                    withSampleRate:(int)sampleRate
                    withVideoWidth:(int)videoWidth
                   withVideoHeight:(int)videoHeight
                     withFrameRate:(int)frameRate
                       withAudioCb:(MediaCapture::on_audio_cb &&)audioCb
                       withVideoCb:(MediaCapture::on_video_cb &&)videoCb {
  self = [super init];

  if (self) {
    NSError *err;

    _session = [[AVCaptureSession alloc] init];
    _audioOutput = [[AVCaptureAudioDataOutput alloc] init];
    _videoOutput = [[AVCaptureVideoDataOutput alloc] init];
    _audioDispatchQueue = dispatch_queue_create(nil, DISPATCH_QUEUE_SERIAL);
    _videoDispatchQueue = dispatch_queue_create(nil, DISPATCH_QUEUE_SERIAL);
    _audioCb = std::move(audioCb);
    _videoCb = std::move(videoCb);

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

    AVCaptureDevice *videoDevice = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if (!videoDevice) {
      NSLog(@"No available video device");
      return nil;
    }

    BOOL videoConfSupported = NO;
    if ([videoDevice lockForConfiguration:&err]) {
      for (AVCaptureDeviceFormat *format in videoDevice.formats) {
        CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
        if (dimensions.width == videoWidth && dimensions.height == videoHeight) {
          CMTime targetFrameDuration = CMTimeMake(1, frameRate);
          for (AVFrameRateRange *range in format.videoSupportedFrameRateRanges) {
            if (CMTimeCompare(targetFrameDuration, range.minFrameDuration) >= 0 &&
                CMTimeCompare(targetFrameDuration, range.maxFrameDuration) <= 0) {
              videoDevice.activeFormat = format;
              [videoDevice setActiveVideoMinFrameDuration:targetFrameDuration];
              [videoDevice setActiveVideoMaxFrameDuration:targetFrameDuration];
              videoConfSupported = YES;
              break;
            }
          }
        }
        if (videoConfSupported) { break; }
      }
      [videoDevice unlockForConfiguration];
    } else {
      NSLog(@"Fail to lock video device for configuration: %@", err.localizedDescription);
      return nil;
    }
    if (!videoConfSupported) {
      NSLog(@"Video config not supported");
      return nil;
    }

    AVCaptureDeviceInput *videoInput = [AVCaptureDeviceInput deviceInputWithDevice:videoDevice error:&err];
    if (!videoInput) {
      NSLog(@"Fail to create video input: %@", err.localizedDescription);
      return nil;
    }

    // Audio settings
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

    // Video settings
    NSDictionary *videoSettings = @{
      (id) kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8Planar),
      (id) kCVPixelBufferWidthKey: @(videoWidth),
      (id) kCVPixelBufferHeightKey: @(videoHeight),
    };
    [_videoOutput setVideoSettings:videoSettings];

    [_videoOutput setSampleBufferDelegate:self queue:_videoDispatchQueue];

    // Session configs
    [_session beginConfiguration];

    if (![_session canAddInput:audioInput]) {
      NSLog(@"Cannot add audio input");
      return nil;
    }
    [_session addInput:audioInput];

    if (![_session canAddInput:videoInput]) {
      NSLog(@"Cannot add video input");
      return nil;
    }
    [_session addInput:videoInput];

    if (![_session canAddOutput:_audioOutput]) {
      NSLog(@"Cannot add audio output");
      return nil;
    }
    [_session addOutput:_audioOutput];

    if (![_session canAddOutput:_videoOutput]) {
      NSLog(@"Cannot add video output");
      return nil;
    }
    [_session addOutput:_videoOutput];

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
        _audioCb((unsigned char *) data, (int) samples);
      }
    }
  } else if (output == _videoOutput) {
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (imageBuffer) {
      unsigned char *planes[3] = {NULL};
      int strides[3] = {0};
      size_t planeCount = CVPixelBufferGetPlaneCount(imageBuffer);
      CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
      for (size_t i = 0; i != 3 && i < planeCount; ++i) {
        planes[i] = (unsigned char *) CVPixelBufferGetBaseAddressOfPlane(imageBuffer, i);
        strides[i] = (int) CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, i);
      }
      _videoCb(planes, strides);
      CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
    }
  }
}

@end

struct MediaCapture::Impl final {
  __strong MediaCaptureImpl* instance;

  Impl(int nb_channels, int sample_rate, int width, int height, int frame_rate,
       on_audio_cb&& on_audio, on_video_cb&& on_video)
      : instance([[MediaCaptureImpl alloc] initWithChannelNum:nb_channels
                                               withSampleRate:sample_rate
                                               withVideoWidth:width
                                              withVideoHeight:height
                                                withFrameRate:frame_rate
                                                  withAudioCb:(std::move(on_audio))
                                                  withVideoCb:(std::move(on_video))]) {
    if (!instance) {
      throw std::runtime_error("MediaCapture::Impl: fail to init MediaCaptureImpl");
    }
  }

  ~Impl() = default;
};

MediaCapture::MediaCapture(int nb_channels, int sample_rate,
                           int width, int height, int frame_rate,
                           on_audio_cb&& on_audio, on_video_cb&& on_video)
    : impl_(std::make_unique<Impl>(nb_channels, sample_rate,
                                   width, height, frame_rate,
                                   std::move(on_audio),
                                   std::move(on_video))) { }

MediaCapture::~MediaCapture() = default;

void MediaCapture::start() {
  [impl_->instance start];
}

void MediaCapture::stop() {
  [impl_->instance stop];
}
