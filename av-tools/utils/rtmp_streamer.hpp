//
//  rtmp_streamer.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/9/17.
//

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include <librtmp/rtmp.h>

namespace av {

namespace utils {

class RTMPStreamer {
 public:
  RTMPStreamer(std::string_view url)
      : url_(url), r_(RTMP_Alloc(), &RTMP_Free) {
    if (!r_) {
      throw std::runtime_error("RTMPStreamer: error allocating RTMP");
    }

    RTMP_Init(r_.get());

    if (!RTMP_SetupURL(r_.get(), const_cast<char*>(url_.c_str()))) {
      throw std::runtime_error("RTMPStreamer: error setting URL");
    }

    RTMP_EnableWrite(r_.get());
  }

  ~RTMPStreamer() {
    RTMP_Close(r_.get());
  }

  bool connect() {
    if (!RTMP_Connect(r_.get(), nullptr)) {
      return false;
    }

    if (!RTMP_ConnectStream(r_.get(), 0)) {
      return false;
    }

    return true;
  }

  int write(const uint8_t* buf, int size) {
    return RTMP_Write(r_.get(), reinterpret_cast<const char*>(buf), size);
  }

 private:
  const std::string url_;
  std::unique_ptr<RTMP, decltype(&RTMP_Free)> r_;
};

}

}
