//
//  ffmpeg_helper.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/14.
//

#ifndef ffmpeg_helper_hpp
#define ffmpeg_helper_hpp

#include <functional>
#include <memory>
#include <vector>
#include "avcodec.hpp"
#include "avformat.hpp"

namespace av {

struct EncodeHelper {
  using Muxer = ffmpeg::Muxer;
  using Encoder = ffmpeg::Encoder;
  using on_write_cb = std::function<bool(std::size_t, AVFrame*, AVPacket*)>;
  using on_new_stream_cb = std::function<void(Muxer&, Encoder&, AVStream*, std::size_t)>;

  static std::unique_ptr<EncodeHelper>
  FromCodecID(const char* filename,
              const std::vector<AVCodecID>& ids,
              on_new_stream_cb&& on_new_stream);

  EncodeHelper(const char* filename);

  int write_frame(std::size_t stream_id, AVFrame* frame, on_write_cb&& on_write);

  static constexpr auto pkt_deleter = [](AVPacket* pkt) { av_packet_free(&pkt); };
  std::unique_ptr<AVPacket, decltype(pkt_deleter)> pkt_;
  Muxer muxer_;
  std::vector<Encoder> encoders_;
};

} // av

#endif /* ffmpeg_helper_hpp */
