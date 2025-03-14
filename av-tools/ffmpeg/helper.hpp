//
//  helper.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/14.
//

#ifndef helper_hpp
#define helper_hpp

#include <functional>
#include <vector>
#include "avcodec.hpp"
#include "avformat.hpp"

namespace av {

namespace ffmpeg {

struct DecodeHelper {
  using on_frame_cb = std::function<void(AVFrame*)>;

  DecodeHelper(const char* filename);

  int play(int stream_index, on_frame_cb&& on_frame);

  Demuxer demuxer_;
  std::vector<Decoder> decoders_;
};

} // ffmpeg

} // av

#endif /* helper_hpp */
