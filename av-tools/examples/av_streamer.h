//
//  av_streamer.h
//  av-tools
//
//  Created by zhanwang-sky on 2025/3/31.
//

#ifndef av_streamer_h
#define av_streamer_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct av_streamer av_streamer_t;

av_streamer_t* av_streamer_alloc(const char* url, int sample_rate, int nb_channels);

void av_streamer_free(av_streamer_t* p_streamer);

int av_streamer_write_samples(av_streamer_t* p_streamer,
                              const unsigned char* audio_data,
                              int nb_samples);

#ifdef __cplusplus
}
#endif

#endif /* av_streamer_h */
