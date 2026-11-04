/*  Video Source (Camera)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/VideoPipeline/Backends/CameraImplementations.h"
#include "CommonFramework/VideoPipeline/VideoSource.h"
#include "VideoSource_Camera.h"
#ifdef _WIN32
#include "CommonFramework/VideoPipeline/Backends/CameraVideoSource_DirectShow.h"
#endif

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{



bool VideoSourceDescriptor_Camera::operator==(const VideoSourceDescriptor& x) const{
    if (typeid(*this) != typeid(x)){
        return false;
    }
    return m_info == static_cast<const VideoSourceDescriptor_Camera&>(x).m_info;
}
std::string VideoSourceDescriptor_Camera::display_name() const{
    return m_info.display_name();
}

JsonValue VideoSourceDescriptor_Camera::to_json() const{
    return m_info.device_name();
}
void VideoSourceDescriptor_Camera::load_json(const JsonValue& json){
    const std::string* name = json.to_string();
    if (name != nullptr){
        m_info = CameraInfo(*name);
    }
}

std::unique_ptr<VideoSource> VideoSourceDescriptor_Camera::make_VideoSource(
    Logger& logger,
    Resolution resolution,
    VideoFormat format,
    FramesPerSecond fps
) const{
#ifdef _WIN32
    if (auto endpoint = DirectShow::parse_endpoint(m_info.device_name())){
        if (endpoint->kind != DirectShow::EndpointKind::Video || !endpoint->pin.empty()){
            throw std::invalid_argument("Unsupported DirectShow video endpoint.");
        }
        return std::make_unique<CameraVideoSource_DirectShow>(logger, endpoint->device, resolution, format, fps);
    }
#endif
//    cout << "format = " << (int)format << endl;
    return get_camera_backend().make_video_source(
        logger,
        m_info,
        resolution,
        format,
        fps
    );
}




}


