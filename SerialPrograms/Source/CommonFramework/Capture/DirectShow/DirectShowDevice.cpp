/*  DirectShow Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <charconv>
#include <stdexcept>
#include "DirectShowDevice.h"

namespace PokemonAutomation::DirectShow{
namespace{
constexpr size_t MAX_FIELD_BYTES = 65536;

bool read_field(std::string_view& input, std::string& output){
    const size_t colon = input.find(':');
    if (colon == std::string_view::npos || colon == 0 || colon > 5){
        return false;
    }
    // Canonical encoding: no sign, whitespace or redundant leading zeroes.
    if (colon > 1 && input.front() == '0'){
        return false;
    }
    size_t count = 0;
    auto parsed = std::from_chars(input.data(), input.data() + colon, count);
    if (parsed.ec != std::errc() || parsed.ptr != input.data() + colon || count > MAX_FIELD_BYTES){
        return false;
    }
    input.remove_prefix(colon + 1);
    if (count > input.size()){
        return false;
    }
    output.assign(input.substr(0, count));
    input.remove_prefix(count);
    return output.find('\0') == std::string::npos;
}
}

std::string serialize_endpoint(const Endpoint& endpoint){
    if (endpoint.device.value.empty() || endpoint.device.value.size() > MAX_FIELD_BYTES ||
        endpoint.pin.size() > MAX_FIELD_BYTES || endpoint.device.value.find('\0') != std::string::npos ||
        endpoint.pin.find('\0') != std::string::npos ||
        (endpoint.device.kind != DeviceKeyKind::DevicePath && endpoint.device.kind != DeviceKeyKind::Moniker) ||
        (endpoint.kind != EndpointKind::Video && endpoint.kind != EndpointKind::EmbeddedAudio)){
        throw std::invalid_argument("Invalid DirectShow endpoint identity.");
    }
    return std::string("pa-dshow:1:") +
        (endpoint.device.kind == DeviceKeyKind::DevicePath ? "path:" : "moniker:") +
        (endpoint.kind == EndpointKind::Video ? "video:" : "audio:") +
        std::to_string(endpoint.device.value.size()) + ":" + endpoint.device.value +
        std::to_string(endpoint.pin.size()) + ":" + endpoint.pin;
}

std::optional<Endpoint> parse_endpoint(std::string_view input){
    Endpoint result;
    if (!input.starts_with("pa-dshow:1:")){
        return std::nullopt;
    }
    input.remove_prefix(11);
    if (input.starts_with("path:")){
        result.device.kind = DeviceKeyKind::DevicePath;
        input.remove_prefix(5);
    }else if (input.starts_with("moniker:")){
        result.device.kind = DeviceKeyKind::Moniker;
        input.remove_prefix(8);
    }else{
        return std::nullopt;
    }
    if (input.starts_with("video:")){
        result.kind = EndpointKind::Video;
        input.remove_prefix(6);
    }else if (input.starts_with("audio:")){
        result.kind = EndpointKind::EmbeddedAudio;
        input.remove_prefix(6);
    }else{
        return std::nullopt;
    }
    if (!read_field(input, result.device.value) || result.device.value.empty() ||
        !read_field(input, result.pin) || !input.empty()){
        return std::nullopt;
    }
    return result;
}
}
