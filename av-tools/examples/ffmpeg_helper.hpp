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

struct DecodeHelper {
  using Demuxer = ffmpeg::Demuxer;
  using Decoder = ffmpeg::Decoder;
  using on_read_cb = std::function<bool(unsigned, AVPacket*, AVFrame*)>;

  DecodeHelper(const char* filename);

  int read(on_read_cb&& on_read);

  int flush(unsigned stream_id, on_read_cb&& on_read);

  static constexpr auto frame_deleter = [](AVFrame* frame) { av_frame_free(&frame); };
  static constexpr auto pkt_deleter = [](AVPacket* pkt) { av_packet_free(&pkt); };
  std::unique_ptr<AVFrame, decltype(frame_deleter)> frame_;
  std::unique_ptr<AVPacket, decltype(pkt_deleter)> pkt_;
  Demuxer demuxer_;
  std::vector<Decoder> decoders_;
};

struct EncodeHelper {
  using Muxer = ffmpeg::Muxer;
  using Encoder = ffmpeg::Encoder;
  using on_stream_cb = std::function<void(Muxer&, Encoder&, AVStream*, unsigned)>;
  using on_write_cb = std::function<bool(unsigned, AVFrame*, AVPacket*)>;

  static std::unique_ptr<EncodeHelper>
  FromCodecID(const char* filename,
              const std::vector<AVCodecID>& vid,
              on_stream_cb&& on_stream);

  EncodeHelper(const char* filename);

  int write(unsigned stream_id, AVFrame* frame, on_write_cb&& on_write);

  static constexpr auto pkt_deleter = [](AVPacket* pkt) { av_packet_free(&pkt); };
  std::unique_ptr<AVPacket, decltype(pkt_deleter)> pkt_;
  Muxer muxer_;
  std::vector<Encoder> encoders_;
};

} // av

#endif /* ffmpeg_helper_hpp */
