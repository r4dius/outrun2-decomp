#include "system/title_movie.hpp"
#include "platform/music_stream.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <cstdio>
int main(){
    using namespace outrun::platform;
    if(!avcodec_find_decoder(AV_CODEC_ID_BINKVIDEO)||!avcodec_find_decoder(AV_CODEC_ID_BINKAUDIO_DCT)||!avcodec_find_decoder(AV_CODEC_ID_BINKAUDIO_RDFT)||!av_find_input_format("bink")||!MusicStream::decoder_available())return 1;
    std::string error;TitleMovie movie;
    if(movie.open("/private/tmp/outrun-missing-title.bik",error)||error.empty())return 1;
    MusicStream music;error.clear();
    if(music.open({0,1,2,3},false,error)||music.playing()||error.empty())return 1;
    std::puts("Bink video/audio and Vorbis available; missing/invalid media rejected");return 0;
}
