/*  DirectShow Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_DirectShowSampleSink_H
#define PokemonAutomation_DirectShowSampleSink_H
#ifdef _WIN32
#include "DirectShowNative.h"
#include "DirectShowSampleQueue.h"
namespace PokemonAutomation::DirectShow{
// Private in-process filter: no registry entries, deprecated Sample Grabber,
// UI access, downstream listeners, or per-sample allocations.
ComPtr<IBaseFilter> make_sample_sink(const AM_MEDIA_TYPE& type, std::shared_ptr<SampleQueue> queue);
}
#endif
#endif
