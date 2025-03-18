//
//  ffmpeg_helper.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/14.
//

#ifndef ffmpeg_helper_hpp
#define ffmpeg_helper_hpp

#include <functional>
#include <vector>
#include "avcodec.hpp"
#include "avformat.hpp"

namespace av {

struct DecodeHelper {
  using Demuxer = ffmpeg::Demuxer;
  using Decoder = ffmpeg::Decoder;
  using on_frame_cb = std::function<void(AVFrame*)>;

  DecodeHelper(const char* filename);

  int play(int stream_index, on_frame_cb&& on_frame);

  Demuxer demuxer_;
  std::vector<Decoder> decoders_;
};

} // av

#endif /* ffmpeg_helper_hpp */
