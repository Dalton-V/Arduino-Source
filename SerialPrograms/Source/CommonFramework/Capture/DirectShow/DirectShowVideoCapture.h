/*  DirectShow Video Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_DirectShowVideoCapture_H
#define PokemonAutomation_DirectShowVideoCapture_H
#ifdef _WIN32
#include <memory>
#include <optional>
#include "DirectShowDevice.h"
#include "DirectShowMediaType.h"
#include "DirectShowSampleQueue.h"

namespace PokemonAutomation::DirectShow{
struct VideoMode{
    VideoLayout layout;
    std::string pin;
};
struct VideoRequest{
    size_t width = 0;
    size_t height = 0;
    std::optional<PixelFormat> format;
    size_t fps = 0;
};
size_t fps_label(int64_t interval);

// Synchronous native object confined to one background owner task. COM and
// graph teardown stay on that task. Never construct it on a UI/driver thread.
class VideoCapture{
public:
    VideoCapture(const DeviceKey& key, const VideoRequest& request);
    ~VideoCapture();
    const VideoMode& mode() const;
    const std::vector<VideoMode>& modes() const;
    void start();
    std::shared_ptr<const SampleBuffer> pop();
    void check_events();
private:
    struct Data;
    std::unique_ptr<Data> m_data;
};
}
#endif
#endif
