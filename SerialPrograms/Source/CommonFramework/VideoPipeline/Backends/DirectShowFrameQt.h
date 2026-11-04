/*  DirectShow Video Source
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_DirectShowFrameQt_H
#define PokemonAutomation_DirectShowFrameQt_H
#ifdef _WIN32
#include <QVideoFrame>
#include "CommonFramework/Capture/DirectShow/DirectShowMediaType.h"
#include "CommonFramework/Capture/DirectShow/DirectShowSampleQueue.h"
namespace PokemonAutomation{
QVideoFrame make_directshow_frame(std::shared_ptr<const DirectShow::SampleBuffer> sample, const DirectShow::VideoLayout& layout);
}
#endif
#endif
