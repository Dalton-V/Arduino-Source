/*  DirectShow Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_DirectShowDevice_H
#define PokemonAutomation_DirectShowDevice_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace PokemonAutomation::DirectShow{

// Identity is independent of enumeration order and display names. A moniker
// identity is used only when a driver does not supply DevicePath.
enum class DeviceKeyKind{ DevicePath, Moniker };
enum class EndpointKind{ Video, EmbeddedAudio };
struct DeviceKey{
    DeviceKeyKind kind = DeviceKeyKind::DevicePath;
    std::string value;
    bool operator==(const DeviceKey&) const = default;
};
struct Endpoint{
    DeviceKey device;
    EndpointKind kind = EndpointKind::Video;
    std::string pin;
    bool operator==(const Endpoint&) const = default;
};

// A versioned, length-prefixed encoding allows arbitrary UTF-8 names and pin
// identifiers without delimiter ambiguity. Invalid inputs never partially load.
std::string serialize_endpoint(const Endpoint& endpoint);
std::optional<Endpoint> parse_endpoint(std::string_view serialized);

struct Device{
    DeviceKey key;
    std::string name;
};
struct CatalogSnapshot{
    std::vector<Device> devices;
    std::string error;
};

// Blocking native discovery. Call only on a background task. Does not bind
// filters, probe pins, or acquire a capture device. An empty category succeeds.
CatalogSnapshot enumerate_video_devices();

}
#endif
