/*  DirectShow Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_DirectShowMediaType_H
#define PokemonAutomation_DirectShowMediaType_H
#ifdef _WIN32
#include <optional>
#include <cstddef>
#include <cstdint>
#include "DirectShowNative.h"

namespace PokemonAutomation::DirectShow{
enum class PixelFormat{ BGR24, BGR32, YUY2, NV12 };
struct VideoLayout{
    PixelFormat format;
    size_t width;
    size_t height;
    size_t stride;
    size_t bytes;
    bool bottom_up;
    int64_t interval;
};
enum class SampleFormat{ UInt8, Int16, Int32, Float32 };
struct AudioLayout{
    SampleFormat format;
    size_t rate;
    size_t channels;
    size_t block_bytes;
    uint32_t channel_mask;
};
// Strict parsers for progressive SDR video and uncompressed audio only.
// Reject malformed headers, unknown layouts, and unsupported representations.
std::optional<VideoLayout> video_layout(const AM_MEDIA_TYPE& type);
std::optional<AudioLayout> audio_layout(const AM_MEDIA_TYPE& type);
}
#endif
#endif
