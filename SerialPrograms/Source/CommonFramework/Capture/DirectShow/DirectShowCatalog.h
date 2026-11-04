/*  DirectShow Device Catalog
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_DirectShowCatalog_H
#define PokemonAutomation_DirectShowCatalog_H
#ifdef _WIN32
#include "DirectShowDevice.h"
namespace PokemonAutomation::DirectShow{
// Non-blocking identity discovery. Refreshes off the UI and never binds filters.
CatalogSnapshot cached_video_devices();
// Call before explicitly stopping the global pools at application shutdown.
void stop_video_catalog();
}
#endif
#endif
