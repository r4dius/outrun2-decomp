#include "title_movie.hpp"
#include <cmath>
#if defined(OUTRUN_WITH_FFMPEG)
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
namespace outrun::platform {
namespace {
struct Decoder {
    AVFormatContext* format{};AVCodecContext* codec{};AVFrame* frame{};
    AVPacket* packet{};SwsContext* scale{};int stream{-1};double fps{};
    bool draining{};
    AVCodecContext* audio{};AVFrame* audio_frame{};SwrContext* resample{};int audio_stream{-1};
    ~Decoder(){sws_freeContext(scale);av_packet_free(&packet);av_frame_free(&frame);
        swr_free(&resample);av_frame_free(&audio_frame);avcodec_free_context(&audio);
        avcodec_free_context(&codec);avformat_close_input(&format);}
};
bool fail(std::string& error,const char* message){error=message;return false;}
}
TitleMovie::~TitleMovie(){close();}
void TitleMovie::close(){delete static_cast<Decoder*>(impl_);impl_=nullptr;pixels_.clear();audio_.clear();width_=height_=0u;decoded_=audio_frames_=0u;has_audio_=false;}
bool TitleMovie::open(const std::string& path,std::string& error){
    close();error.clear();auto* d=new Decoder;impl_=d;
    // "sdmc:/..." would be parsed by libavformat as a URL scheme: name the
    // file protocol explicitly for device-prefixed paths.
    const std::string url=path.find(':')!=std::string::npos&&path.compare(0,5,"file:")!=0?"file:"+path:path;
    if(avformat_open_input(&d->format,url.c_str(),nullptr,nullptr)<0||avformat_find_stream_info(d->format,nullptr)<0){close();return fail(error,"cannot open original title movie");}
    d->stream=av_find_best_stream(d->format,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);
    if(d->stream<0){close();return fail(error,"title movie has no video stream");}
    const auto* stream=d->format->streams[d->stream];
    // This path consumes the owned retail Bink, never a substitute image.
    if(stream->codecpar->codec_id!=AV_CODEC_ID_BINKVIDEO){close();return fail(error,"title movie is not retail Bink video");}
    const auto* codec=avcodec_find_decoder(stream->codecpar->codec_id);
    if(!codec||(d->codec=avcodec_alloc_context3(codec))==nullptr||
       avcodec_parameters_to_context(d->codec,stream->codecpar)<0||avcodec_open2(d->codec,codec,nullptr)<0){close();return fail(error,"Bink decoder initialization failed");}
    if(d->codec->width<=0||d->codec->height<=0||d->codec->width>1920||d->codec->height>1080){close();return fail(error,"title movie dimensions outside bounds");}
    width_=unsigned(d->codec->width);height_=unsigned(d->codec->height);
    d->fps=av_q2d(stream->avg_frame_rate);
    if(!std::isfinite(d->fps)||d->fps<=0.0||d->fps>120.0){close();return fail(error,"invalid title movie rate");}
    d->frame=av_frame_alloc();d->packet=av_packet_alloc();
    if(!d->frame||!d->packet){close();return fail(error,"title movie allocation failed");}
    d->audio_stream=av_find_best_stream(d->format,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);
    if(d->audio_stream>=0){
        const auto* parameters=d->format->streams[d->audio_stream]->codecpar;
        const auto* decoder=avcodec_find_decoder(parameters->codec_id);
        if(!decoder||(d->audio=avcodec_alloc_context3(decoder))==nullptr||
           avcodec_parameters_to_context(d->audio,parameters)<0||avcodec_open2(d->audio,decoder,nullptr)<0||
           !(d->audio_frame=av_frame_alloc())){close();return fail(error,"Bink audio initialization failed");}
        AVChannelLayout stereo=AV_CHANNEL_LAYOUT_STEREO;
        if(swr_alloc_set_opts2(&d->resample,&stereo,AV_SAMPLE_FMT_S16,48000,
            &d->audio->ch_layout,d->audio->sample_fmt,d->audio->sample_rate,0,nullptr)<0||
            swr_init(d->resample)<0){close();return fail(error,"Bink audio resampling initialization failed");}
        has_audio_=true;
    }
    pixels_.resize(std::size_t(width_)*height_*4u);
    if(!advance(0.0,error)){close();return false;}return true;
}
bool TitleMovie::advance(double elapsed,std::string& error){
    error.clear();auto* d=static_cast<Decoder*>(impl_);
    if(!d||!std::isfinite(elapsed)||elapsed<0.0||elapsed>86400.0)return fail(error,"invalid title movie clock");
    const auto target=std::uint64_t(elapsed*d->fps)+1u;
    auto receive_audio=[&](){
        for(;;){
            const int result=avcodec_receive_frame(d->audio,d->audio_frame);
            if(result==AVERROR(EAGAIN)||result==AVERROR_EOF)return true;
            if(result<0)return fail(error,"Bink audio decode failed");
            const int capacity=swr_get_out_samples(d->resample,d->audio_frame->nb_samples);
            if(capacity<0||capacity>96000||audio_.size()+std::size_t(capacity)*2u>192000u)
                return fail(error,"title audio consumer stalled");
            const auto base=audio_.size();audio_.resize(base+std::size_t(capacity)*2u);
            auto* output=reinterpret_cast<std::uint8_t*>(audio_.data()+base);
            const int samples=swr_convert(d->resample,&output,capacity,
                const_cast<const std::uint8_t**>(d->audio_frame->extended_data),d->audio_frame->nb_samples);
            av_frame_unref(d->audio_frame);
            if(samples<0)return fail(error,"Bink audio conversion failed");
            audio_.resize(base+std::size_t(samples)*2u);audio_frames_+=unsigned(samples);
        }
    };
    // Bounded catch-up: do not freeze input after a long scheduling stall.
    unsigned budget=8u;
    while(decoded_<target&&budget--){
        unsigned rewinds=0u;
        for(;;){
            const int rc=avcodec_receive_frame(d->codec,d->frame);
            if(rc==0)break;
            if(rc==AVERROR_EOF){
                if(++rewinds>1u)return fail(error,"title movie contains no decodable frames");
                if(av_seek_frame(d->format,d->stream,0,AVSEEK_FLAG_BACKWARD)<0)return fail(error,"title movie rewind failed");
                if(d->audio){avcodec_flush_buffers(d->audio);swr_close(d->resample);
                    if(swr_init(d->resample)<0)return fail(error,"Bink audio rewind failed");}
                avcodec_flush_buffers(d->codec);d->draining=false;continue;
            }
            if(rc!=AVERROR(EAGAIN))return fail(error,"title movie decode failed");
            if(d->draining)return fail(error,"title movie stalled while draining");
            int read{};
            do{
                av_packet_unref(d->packet);read=av_read_frame(d->format,d->packet);
                if(read>=0&&d->packet->stream_index==d->audio_stream){
                    if(avcodec_send_packet(d->audio,d->packet)<0||!receive_audio())return fail(error,"Bink audio packet rejected");
                }
            }while(read>=0&&d->packet->stream_index!=d->stream);
            if(read<0){
                if(read!=AVERROR_EOF)return fail(error,"title movie read failed");
                d->draining=true;
                if(avcodec_send_packet(d->codec,nullptr)<0)return fail(error,"title movie drain failed");
            }else{
                const int sent=avcodec_send_packet(d->codec,d->packet);
                av_packet_unref(d->packet);
                if(sent<0)return fail(error,"title movie packet rejected");
            }
        }
        if(d->frame->width!=int(width_)||d->frame->height!=int(height_))return fail(error,"title movie changed dimensions");
        d->scale=sws_getCachedContext(d->scale,int(width_),int(height_),AVPixelFormat(d->frame->format),
            int(width_),int(height_),AV_PIX_FMT_RGBA,SWS_BILINEAR,nullptr,nullptr,nullptr);
        if(!d->scale)return fail(error,"title movie color conversion failed");
        std::uint8_t* out[4]{pixels_.data(),nullptr,nullptr,nullptr};int strides[4]{int(width_*4u),0,0,0};
        if(sws_scale(d->scale,d->frame->data,d->frame->linesize,0,int(height_),out,strides)!=int(height_))return fail(error,"title movie incomplete RGBA frame");
        av_frame_unref(d->frame);++decoded_;
    }
    return true;
}
}
#else
namespace outrun::platform {
TitleMovie::~TitleMovie()=default;
void TitleMovie::close(){impl_=nullptr;pixels_.clear();audio_.clear();width_=height_=0u;decoded_=audio_frames_=0u;has_audio_=false;}
bool TitleMovie::open(const std::string&,std::string& error){error="FFmpeg support not compiled";return false;}
bool TitleMovie::advance(double,std::string& error){error="FFmpeg support not compiled";return false;}
}
#endif
