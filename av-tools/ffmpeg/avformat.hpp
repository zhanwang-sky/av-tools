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

class AVOFormat : public AVFormatBase {
 public:
  AVOFormat();

  AVOFormat(const char* url, const char* format = nullptr);

  virtual ~AVOFormat();

  void open(const char* url, const char* format = nullptr);

  void close();

  AVStream* new_stream();

  int write_header(AVDictionary** opts);

  int write_frame(AVPacket* pkt);

  int interleaved_write_frame(AVPacket* pkt);

 private:
  bool need_trailer_ = false;
  bool need_close_ = false;
};

} // ffmpeg

} // av

#endif /* avformat_hpp */
