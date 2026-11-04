/*  DirectShow Video Source
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_CameraVideoSource_DirectShow_H
#define PokemonAutomation_CameraVideoSource_DirectShow_H
#ifdef _WIN32
#include "Common/Cpp/Concurrency/AsyncTask.h"
#include "Common/Cpp/Concurrency/ConditionVariable.h"
#include "CommonFramework/Capture/DirectShow/DirectShowVideoCapture.h"
#include "CommonFramework/VideoPipeline/VideoSource.h"
#include "QVideoFrameCache.h"
#include "SnapshotManager.h"

namespace PokemonAutomation{
class CameraVideoSource_DirectShow final : public VideoSource{
public:
    CameraVideoSource_DirectShow(Logger& logger, DirectShow::DeviceKey key, Resolution resolution, VideoFormat format, FramesPerSecond fps);
    ~CameraVideoSource_DirectShow();
    bool has_async_metadata() const override{ return true; }
    Resolution current_resolution() const override;
    VideoFormat current_format() const override;
    FramesPerSecond current_fps() const override;
    const VideoFormatSet& supported_formats() const override;
    VideoSnapshot snapshot_latest_blocking() override;
    VideoSnapshot snapshot_recent_nonblocking(WallClock min_time) override;
    QWidget* make_display_QtWidget(QWidget* parent) override;
    bool latest_frame(QVideoFrame& frame, uint64_t& sequence);
    void rendered(WallClock timestamp){ report_rendered_frame(timestamp); }
private:
    void run(DirectShow::DeviceKey key, Resolution desired, VideoFormat format, FramesPerSecond fps) noexcept;
    struct Metadata{
        Resolution resolution;
        VideoFormat format;
        FramesPerSecond fps;
        VideoFormatSet formats;
    };
    Logger& m_logger;
    mutable Mutex m_lock;
    ConditionVariable m_cv;
    bool m_stopping = false;
    bool m_running = false;
    // Assigned once after negotiation; never mutated after publication.
    std::unique_ptr<const Metadata> m_metadata;
    const Resolution m_requested_resolution;
    QVideoFrameCache m_cache;
    SnapshotManager m_snapshots;
    AsyncTask m_task;
};
}
#endif
#endif
