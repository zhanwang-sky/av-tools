//
//  avformat.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#ifndef avformat_hpp
#define avformat_hpp

extern "C" {
#include <libavformat/avformat.h>
}

namespace av {

namespace ffmpeg {

class AVFormatBase {
 public:
  AVFormatBase(const AVFormatBase&) = delete;
  AVFormatBase& operator=(const AVFormatBase&) = delete;

  AVFormatContext* ctx() const {
    return ctx_;
  }

 protected:
  AVFormatBase() { }

  virtual ~AVFormatBase() { }

  AVFormatContext* ctx_ = nullptr;
};

class AVDemuxer : public AVFormatBase {
 public:
  AVDemuxer();

  AVDemuxer(const char* url,
            const AVInputFormat* fmt = nullptr,
            AVDictionary** opts = nullptr);

  virtual ~AVDemuxer();

  void open(const char* url,
            const AVInputFormat* fmt = nullptr,
            AVDictionary** opts = nullptr);

  void close();

  int read_frame(AVPacket* pkt);
};

class AVMuxer : public AVFormatBase {
 public:
  AVMuxer();

  AVMuxer(const char* url,
          const char* fmt_name = nullptr,
          const AVOutputFormat* fmt = nullptr);

  virtual ~AVMuxer();

  void open(const char* url,
            const char* fmt_name = nullptr,
            const AVOutputFormat* fmt = nullptr);

  void close();

  AVStream* new_stream();

  int write_header(AVDictionary** opts = nullptr);

  int write_frame(AVPacket* pkt);

  int interleaved_write_frame(AVPacket* pkt);

 private:
  bool need_trailer_ = false;
  bool need_close_ = false;
};

} // ffmpeg

} // av

#endif /* avformat_hpp */
