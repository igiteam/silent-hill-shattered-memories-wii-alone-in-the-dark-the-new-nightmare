#include "video.h"

#include "audio.h"

#if NEWNIGHTMARE_VIDEO

// FFmpeg's headers, without the warnings of their own code
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <deque>

namespace {

struct Picture {
    double milliseconds = 0;
    int width = 0, height = 0;
    std::vector<uint32_t> rgba;
};

AVFormatContext* s_format = nullptr;
AVCodecContext* s_video = nullptr;
AVCodecContext* s_audio = nullptr;
int s_videoStream = -1, s_audioStream = -1;
SwsContext* s_scale = nullptr;
SwrContext* s_resample = nullptr;
AVPacket* s_packet = nullptr;
AVFrame* s_frame = nullptr;
std::deque<Picture> s_pictures;  // decoded, waiting for their time
bool s_ended = false;            // no more packets

constexpr int kRate = 44100;

// The pictures of a decoded frame, as RGBA
void keepPicture(const AVFrame* f)
{
    Picture p;
    p.width = f->width;
    p.height = f->height;
    const AVRational base = s_format->streams[s_videoStream]->time_base;
    const int64_t pts = f->best_effort_timestamp != AV_NOPTS_VALUE ? f->best_effort_timestamp : f->pts;
    p.milliseconds = pts != AV_NOPTS_VALUE ? double(pts) * av_q2d(base) * 1000.0 : 0.0;
    s_scale = sws_getCachedContext(s_scale, f->width, f->height, AVPixelFormat(f->format), f->width, f->height,
                                   AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!s_scale)
        return;
    p.rgba.resize(size_t(f->width) * size_t(f->height));
    uint8_t* planes[4] = { reinterpret_cast<uint8_t*>(p.rgba.data()), nullptr, nullptr, nullptr };
    int strides[4] = { f->width * 4, 0, 0, 0 };
    sws_scale(s_scale, f->data, f->linesize, 0, f->height, planes, strides);
    s_pictures.push_back(std::move(p));
}

// The sound of a decoded frame, to the stream (stereo float at 44100)
void playSound(const AVFrame* f)
{
    if (!s_resample) {
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        if (swr_alloc_set_opts2(&s_resample, &stereo, AV_SAMPLE_FMT_FLT, kRate, &f->ch_layout,
                                AVSampleFormat(f->format), f->sample_rate, 0, nullptr) < 0
            || swr_init(s_resample) < 0)
            return;
    }
    const int most = swr_get_out_samples(s_resample, f->nb_samples);
    std::vector<float> out(size_t(std::max(most, 0)) * 2);
    uint8_t* planes[1] = { reinterpret_cast<uint8_t*>(out.data()) };
    const int made = swr_convert(s_resample, planes, most, const_cast<const uint8_t**>(f->extended_data), f->nb_samples);
    if (made > 0)
        putStreamAudio(out.data(), made);
}

// Every frame the decoder of a stream has ready
void drain(AVCodecContext* codec, bool video)
{
    while (avcodec_receive_frame(codec, s_frame) == 0) {
        if (video)
            keepPicture(s_frame);
        else
            playSound(s_frame);
        av_frame_unref(s_frame);
    }
}

// Reads packets until a picture is waiting (or the end)
void readAhead()
{
    while (!s_ended && s_pictures.size() < 2) {
        if (av_read_frame(s_format, s_packet) < 0) {
            s_ended = true;
            if (s_video) {
                avcodec_send_packet(s_video, nullptr);
                drain(s_video, true);
            }
            break;
        }
        if (s_packet->stream_index == s_videoStream && s_video) {
            if (avcodec_send_packet(s_video, s_packet) == 0)
                drain(s_video, true);
        } else if (s_packet->stream_index == s_audioStream && s_audio) {
            if (avcodec_send_packet(s_audio, s_packet) == 0)
                drain(s_audio, false);
        }
        av_packet_unref(s_packet);
    }
}

AVCodecContext* openDecoder(int stream)
{
    if (stream < 0)
        return nullptr;
    const AVCodecParameters* parameters = s_format->streams[stream]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(parameters->codec_id);
    if (!codec)
        return nullptr;
    AVCodecContext* context = avcodec_alloc_context3(codec);
    if (!context || avcodec_parameters_to_context(context, parameters) < 0 || avcodec_open2(context, codec, nullptr) < 0) {
        avcodec_free_context(&context);
        return nullptr;
    }
    return context;
}

} // namespace

bool openVideo(const std::string& path)
{
    closeVideo();
    if (avformat_open_input(&s_format, path.c_str(), nullptr, nullptr) < 0)
        return false;
    if (avformat_find_stream_info(s_format, nullptr) < 0) {
        closeVideo();
        return false;
    }
    s_videoStream = av_find_best_stream(s_format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    s_audioStream = av_find_best_stream(s_format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    s_video = openDecoder(s_videoStream);
    s_audio = openDecoder(s_audioStream);
    if (!s_video) {
        closeVideo();
        return false;
    }
    s_packet = av_packet_alloc();
    s_frame = av_frame_alloc();
    s_ended = false;
    if (s_audio)
        startStreamAudio();
    readAhead();
    return true;
}

bool videoPicture(double milliseconds, int& width, int& height, std::vector<uint32_t>& rgba)
{
    if (!s_format)
        return false;
    readAhead();
    bool shown = false;
    // The newest picture whose time has come (late ones are skipped)
    while (!s_pictures.empty() && s_pictures.front().milliseconds <= milliseconds) {
        Picture& p = s_pictures.front();
        width = p.width;
        height = p.height;
        rgba.swap(p.rgba);
        s_pictures.pop_front();
        shown = true;
        readAhead();
    }
    return shown;
}

bool videoFinished()
{
    return !s_format || (s_ended && s_pictures.empty());
}

void closeVideo()
{
    stopStreamAudio();
    s_pictures.clear();
    if (s_scale)
        sws_freeContext(s_scale);
    s_scale = nullptr;
    swr_free(&s_resample);
    avcodec_free_context(&s_video);
    avcodec_free_context(&s_audio);
    av_packet_free(&s_packet);
    av_frame_free(&s_frame);
    if (s_format)
        avformat_close_input(&s_format);
    s_videoStream = s_audioStream = -1;
    s_ended = false;
}

#else

bool openVideo(const std::string&)
{
    return false;
}

bool videoPicture(double, int&, int&, std::vector<uint32_t>&)
{
    return false;
}

bool videoFinished()
{
    return true;
}

void closeVideo()
{
}

#endif
