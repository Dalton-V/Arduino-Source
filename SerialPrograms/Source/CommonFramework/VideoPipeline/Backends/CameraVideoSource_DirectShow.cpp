/*  DirectShow Video Source
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifdef _WIN32
#include <QQuickItem>
#include <QTimer>
#include <QVideoSink>
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "CameraWidgetQt6_QQuickWidget.h"
#include "CameraVideoSource_DirectShow.h"
#include "DirectShowFrameQt.h"
#include "VideoFrameQt.h"

namespace PokemonAutomation{
namespace{
VideoFormat public_format(DirectShow::PixelFormat format){
    switch (format){
    case DirectShow::PixelFormat::NV12: return VideoFormat::NV12;
    case DirectShow::PixelFormat::YUY2: return VideoFormat::YUYV;
    default: return VideoFormat::OTHER;
    }
}
}
CameraVideoSource_DirectShow::CameraVideoSource_DirectShow(Logger& logger, DirectShow::DeviceKey key,
    Resolution resolution, VideoFormat format, FramesPerSecond fps)
    : VideoSource(logger, false)
    , m_logger(logger)
    , m_requested_resolution(resolution ? resolution : Resolution(1920, 1080))
    , m_cache(logger)
    , m_snapshots(logger, m_cache)
{
    m_task = GlobalThreadPools::unlimited_normal().dispatch([this, key = std::move(key), resolution, format, fps]{
        run(key, resolution, format, fps);
    });
}
CameraVideoSource_DirectShow::~CameraVideoSource_DirectShow(){
    {
        std::lock_guard<Mutex> lock(m_lock);
        m_stopping = true;
        m_cv.notify_all();
    }
    m_task.wait_and_ignore_exceptions();
}
Resolution CameraVideoSource_DirectShow::current_resolution() const{
    std::lock_guard<Mutex> lock(m_lock);
    return m_metadata ? m_metadata->resolution : m_requested_resolution;
}
VideoFormat CameraVideoSource_DirectShow::current_format() const{
    std::lock_guard<Mutex> lock(m_lock);
    return m_metadata ? m_metadata->format : VideoFormat::OTHER;
}
FramesPerSecond CameraVideoSource_DirectShow::current_fps() const{
    std::lock_guard<Mutex> lock(m_lock);
    return m_metadata ? m_metadata->fps : 0;
}
const VideoFormatSet& CameraVideoSource_DirectShow::supported_formats() const{
    static const VideoFormatSet empty;
    std::lock_guard<Mutex> lock(m_lock);
    return m_metadata ? m_metadata->formats : empty;
}
void CameraVideoSource_DirectShow::run(DirectShow::DeviceKey key, Resolution desired, VideoFormat format, FramesPerSecond fps) noexcept{
    try{
        DirectShow::VideoRequest request;
        request.width = desired.width;
        request.height = desired.height;
        request.fps = fps;
        if (format == VideoFormat::NV12){
            request.format = DirectShow::PixelFormat::NV12;
        }else if (format == VideoFormat::YUYV){
            request.format = DirectShow::PixelFormat::YUY2;
        }else if (format != VideoFormat::OTHER){
            throw std::runtime_error("This DirectShow backend currently supports NV12, YUYV and RGB video.");
        }
        DirectShow::VideoCapture capture(key, request);
        const auto layout = capture.mode().layout;
        auto metadata = std::make_unique<Metadata>();
        metadata->resolution = Resolution(layout.width, layout.height);
        metadata->format = public_format(layout.format);
        metadata->fps = DirectShow::fps_label(layout.interval);
        for (const auto& mode : capture.modes()){
            const auto& supported = mode.layout;
            metadata->formats[{supported.width, supported.height}][public_format(supported.format)].insert(DirectShow::fps_label(supported.interval));
        }
        {
            std::lock_guard<Mutex> lock(m_lock);
            if (m_stopping){
                return;
            }
            m_metadata = std::move(metadata);
        }
        m_logger.log("DirectShow camera opened at " + std::to_string(layout.width) + " x " + std::to_string(layout.height) + ".");
        capture.start();
        int64_t last_presentation = -1;
        auto last_sample = std::chrono::steady_clock::now();
        while (true){
            {
                std::unique_lock<Mutex> lock(m_lock);
                if (m_stopping){
                    break;
                }
                m_cv.wait_for(lock, std::chrono::milliseconds(5), [this]{ return m_stopping; });
                if (m_stopping){
                    break;
                }
            }
            capture.check_events();
            auto sample = capture.pop();
            if (!sample){
                if (std::chrono::steady_clock::now() - last_sample > std::chrono::seconds(10)){
                    throw std::runtime_error("No frames arrived from DirectShow for 10 seconds. Check OBS Virtual Camera, then use Reset Video.");
                }
                continue;
            }
            last_sample = std::chrono::steady_clock::now();
            const WallClock arrival = sample->arrival;
            int64_t presentation = (std::max)(last_presentation + 1, sample->start / 10);
            last_presentation = presentation;
            QVideoFrame frame = make_directshow_frame(std::move(sample), layout);
            frame.setStartTime(presentation);
            frame.setEndTime(presentation + (std::max)(int64_t(1), layout.interval / 10));
            if (!m_cache.push_frame(frame, arrival)){
                continue;
            }
            {
                std::lock_guard<Mutex> lock(m_lock);
                m_running = true;
            }
            report_source_frame(std::make_shared<VideoFrame>(arrival, std::move(frame)));
        }
    }catch (const std::exception& error){
        try{
            m_logger.log("DirectShow: " + std::string(error.what()), COLOR_RED);
        }catch (...){}
    }catch (...){
        try{
            m_logger.log("DirectShow capture failed with an unknown error.", COLOR_RED);
        }catch (...){}
    }
    std::lock_guard<Mutex> lock(m_lock);
    m_running = false;
}
VideoSnapshot CameraVideoSource_DirectShow::snapshot_latest_blocking(){
    {
        std::lock_guard<Mutex> lock(m_lock);
        if (!m_running){
            return {};
        }
    }
    return m_snapshots.snapshot_latest_blocking();
}
VideoSnapshot CameraVideoSource_DirectShow::snapshot_recent_nonblocking(WallClock min_time){
    {
        std::lock_guard<Mutex> lock(m_lock);
        if (!m_running){
            return {};
        }
    }
    VideoSnapshot snapshot = m_snapshots.snapshot_recent_nonblocking(min_time);
    return snapshot.timestamp >= min_time ? snapshot : VideoSnapshot();
}
bool CameraVideoSource_DirectShow::latest_frame(QVideoFrame& frame, uint64_t& sequence){
    std::lock_guard<Mutex> lock(m_lock);
    if (!m_running){
        frame = {};
        return false;
    }
    WallClock timestamp;
    sequence = m_cache.get_latest(frame, timestamp);
    return frame.isValid();
}

QWidget* CameraVideoSource_DirectShow::make_display_QtWidget(QWidget* parent){
    auto* display = new CameraQt6_QQuickWidget::CameraVideoDisplay(parent);
    auto* root = display->rootObject();
    auto* output = root ? root->findChild<QObject*>("videoOutput") : nullptr;
    auto* sink = output ? output->property("videoSink").value<QVideoSink*>() : nullptr;
    if (!sink){
        m_logger.log("DirectShow: unable to attach the accelerated video display.", COLOR_RED);
        return display;
    }
    auto* timer = new QTimer(display);
    timer->setTimerType(Qt::PreciseTimer);
    // Polling the latest frame coalesces display updates: a busy UI cannot
    // accumulate one queued event per captured frame. Capture runs headless too.
    QObject::connect(timer, &QTimer::timeout, display, [this, sink, sequence = uint64_t(0)]() mutable{
        QVideoFrame frame;
        uint64_t latest = 0;
        if (!latest_frame(frame, latest)){
            sink->setVideoFrame({});
            return;
        }
        if (latest == sequence){
            return;
        }
        sequence = latest;
        sink->setVideoFrame(frame);
        report_rendered_frame(current_time());
    });
    timer->start(16);
    return display;
}
}
#endif
