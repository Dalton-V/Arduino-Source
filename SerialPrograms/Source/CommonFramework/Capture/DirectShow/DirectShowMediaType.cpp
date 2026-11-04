/*  DirectShow Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifdef _WIN32
#include <cstring>
#include <limits>
#include "DirectShowMediaType.h"
#include <dvdmedia.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>

namespace PokemonAutomation::DirectShow{
namespace{
template <typename T> bool read(const AM_MEDIA_TYPE& type, T& header){
    if (!type.pbFormat || type.cbFormat < sizeof(T)){
        return false;
    }
    memcpy(&header, type.pbFormat, sizeof(T));
    return true;
}
}

std::optional<VideoLayout> video_layout(const AM_MEDIA_TYPE& type){
    if (type.majortype != MEDIATYPE_Video){
        return std::nullopt;
    }
    BITMAPINFOHEADER bitmap{};
    REFERENCE_TIME interval = 0;
    RECT source{};
    if (type.formattype == FORMAT_VideoInfo){
        VIDEOINFOHEADER header{};
        if (!read(type, header)){
            return std::nullopt;
        }
        bitmap = header.bmiHeader;
        interval = header.AvgTimePerFrame;
        source = header.rcSource;
    }else if (type.formattype == FORMAT_VideoInfo2){
        VIDEOINFOHEADER2 header{};
        if (!read(type, header) || (header.dwInterlaceFlags & AMINTERLACE_IsInterlaced) != 0 ||
            header.dwPictAspectRatioX == 0 || header.dwPictAspectRatioY == 0){
            return std::nullopt;
        }
        bitmap = header.bmiHeader;
        interval = header.AvgTimePerFrame;
        source = header.rcSource;
    }else{
        return std::nullopt;
    }
    // Bound storage and arithmetic before converting signed dimensions.
    if (bitmap.biSize != sizeof(BITMAPINFOHEADER) || bitmap.biWidth <= 0 ||
        bitmap.biHeight == 0 || bitmap.biHeight == (std::numeric_limits<LONG>::min)() ||
        bitmap.biPlanes != 1 || interval <= 0){
        return std::nullopt;
    }
    const size_t width = static_cast<size_t>(bitmap.biWidth);
    const size_t height = static_cast<size_t>(bitmap.biHeight < 0 ? -bitmap.biHeight : bitmap.biHeight);
    if (width > 8192 || height > 8192 ||
        ((source.right != 0 || source.bottom != 0) &&
            (source.left != 0 || source.top != 0 || source.right != bitmap.biWidth ||
                source.bottom != static_cast<LONG>(height)))){
        return std::nullopt;
    }
    VideoLayout result{PixelFormat::BGR24, width, height, 0, 0, false, interval};
    if (type.subtype == MEDIASUBTYPE_RGB24 && bitmap.biCompression == BI_RGB && bitmap.biBitCount == 24){
        result.format = PixelFormat::BGR24;
        result.stride = (width * 3 + 3) & ~size_t(3);
        result.bottom_up = bitmap.biHeight > 0;
    }else if (type.subtype == MEDIASUBTYPE_RGB32 && bitmap.biCompression == BI_RGB && bitmap.biBitCount == 32){
        result.format = PixelFormat::BGR32;
        result.stride = width * 4;
        result.bottom_up = bitmap.biHeight > 0;
    }else if (type.subtype == MEDIASUBTYPE_YUY2 && bitmap.biCompression == MAKEFOURCC('Y','U','Y','2') &&
        bitmap.biBitCount == 16 && width % 2 == 0){
        result.format = PixelFormat::YUY2;
        result.stride = width * 2;
    }else if (type.subtype == MEDIASUBTYPE_NV12 && bitmap.biCompression == MAKEFOURCC('N','V','1','2') &&
        bitmap.biBitCount == 12 && width % 2 == 0 && height % 2 == 0){
        result.format = PixelFormat::NV12;
        result.stride = width;
    }else{
        return std::nullopt;
    }
    result.bytes = result.stride * height;
    if (result.format == PixelFormat::NV12){
        result.bytes += result.stride * (height / 2);
    }
    // Ambiguous padding needs a separately negotiated layout, not a guess.
    if (bitmap.biSizeImage != 0 && bitmap.biSizeImage != result.bytes){
        return std::nullopt;
    }
    return result;
}

std::optional<AudioLayout> audio_layout(const AM_MEDIA_TYPE& type){
    if (type.majortype != MEDIATYPE_Audio || type.formattype != FORMAT_WaveFormatEx){
        return std::nullopt;
    }
    WAVEFORMATEX wave{};
    if (!read(type, wave) || wave.cbSize > type.cbFormat - sizeof(WAVEFORMATEX) ||
        wave.nSamplesPerSec < 8000 || wave.nSamplesPerSec > 192000 ||
        wave.nChannels == 0 || wave.nChannels > 8){
        return std::nullopt;
    }
    bool pcm = wave.wFormatTag == WAVE_FORMAT_PCM && type.subtype == MEDIASUBTYPE_PCM;
    bool floating = wave.wFormatTag == WAVE_FORMAT_IEEE_FLOAT && type.subtype == MEDIASUBTYPE_IEEE_FLOAT;
    uint32_t mask = 0;
    if (wave.wFormatTag == WAVE_FORMAT_EXTENSIBLE){
        WAVEFORMATEXTENSIBLE extended{};
        if (!read(type, extended) || wave.cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) ||
            extended.Samples.wValidBitsPerSample != wave.wBitsPerSample){
            return std::nullopt;
        }
        pcm = extended.SubFormat == KSDATAFORMAT_SUBTYPE_PCM && type.subtype == MEDIASUBTYPE_PCM;
        floating = extended.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT && type.subtype == MEDIASUBTYPE_IEEE_FLOAT;
        mask = extended.dwChannelMask;
        // Unknown or nonstandard channel ordering must be handled explicitly.
        if ((wave.nChannels == 1 && mask != SPEAKER_FRONT_CENTER) ||
            (wave.nChannels == 2 && mask != (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)) || wave.nChannels > 2){
            return std::nullopt;
        }
    }else if (wave.nChannels > 2){
        return std::nullopt;
    }
    SampleFormat format;
    if (pcm && wave.wBitsPerSample == 8){
        format = SampleFormat::UInt8;
    }else if (pcm && wave.wBitsPerSample == 16){
        format = SampleFormat::Int16;
    }else if (pcm && wave.wBitsPerSample == 32){
        format = SampleFormat::Int32;
    }else if (floating && wave.wBitsPerSample == 32){
        format = SampleFormat::Float32;
    }else{
        return std::nullopt;
    }
    const size_t block = wave.nChannels * (wave.wBitsPerSample / 8);
    if (wave.nBlockAlign != block || wave.nAvgBytesPerSec != block * wave.nSamplesPerSec){
        return std::nullopt;
    }
    return AudioLayout{format, wave.nSamplesPerSec, wave.nChannels, block, mask};
}
}
#endif
