/*  DirectShow Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifdef _WIN32
#include <iomanip>
#include <sstream>
#include "Common/Cpp/Strings/Unicode.h"
#include "DirectShowDevice.h"
#include "DirectShowNative.h"

namespace PokemonAutomation::DirectShow{
namespace{
std::string wide_to_utf8(const wchar_t* text, size_t count){
    std::u16string utf16;
    utf16.reserve(count);
    for (size_t i = 0; i < count; i++){
        utf16.push_back(static_cast<char16_t>(text[i]));
    }
    return utr16_to_str(utf16);
}

std::string property(IPropertyBag& bag, const wchar_t* name){
    VARIANT value;
    VariantInit(&value);
    const HRESULT hr = bag.Read(name, &value, nullptr);
    std::string result;
    try{
        if (SUCCEEDED(hr) && value.vt == VT_BSTR && value.bstrVal){
            result = wide_to_utf8(value.bstrVal, SysStringLen(value.bstrVal));
        }
    }catch (...){
        VariantClear(&value);
        throw;
    }
    VariantClear(&value);
    return result;
}

Device describe(IMoniker& moniker){
    Device result;
    ComPtr<IPropertyBag> bag;
    check_hresult(moniker.BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&bag)), "Read DirectShow device properties");
    result.name = property(*bag.Get(), L"FriendlyName");
    result.key.value = property(*bag.Get(), L"DevicePath");
    if (result.key.value.empty()){
        result.key.kind = DeviceKeyKind::Moniker;
        LPOLESTR display = nullptr;
        check_hresult(moniker.GetDisplayName(nullptr, nullptr, &display), "Read DirectShow moniker identity");
        try{
            if (display){
                result.key.value = wide_to_utf8(display, wcslen(display));
            }
        }catch (...){
            CoTaskMemFree(display);
            throw;
        }
        CoTaskMemFree(display);
    }
    if (result.key.value.empty()){
        throw std::runtime_error("DirectShow device has no usable identity.");
    }
    return result;
}

ComPtr<IEnumMoniker> enumerate(){
    ComPtr<ICreateDevEnum> devices;
    check_hresult(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&devices)), "Create DirectShow device enumerator");
    ComPtr<IEnumMoniker> enumerator;
    const HRESULT hr = devices->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumerator, 0);
    check_hresult(hr, "Enumerate DirectShow video devices");
    return enumerator;
}
}

void check_hresult(HRESULT result, const char* operation){
    if (FAILED(result)){
        std::ostringstream message;
        message << operation << ": HRESULT 0x" << std::hex << std::setw(8)
            << std::setfill('0') << static_cast<unsigned long>(result);
        throw std::runtime_error(message.str());
    }
}

CatalogSnapshot enumerate_video_devices(){
    CatalogSnapshot result;
    try{
        ComApartment apartment;
        auto enumerator = enumerate();
        if (!enumerator){
            return result;
        }
        while (true){
            ComPtr<IMoniker> moniker;
            HRESULT hr = enumerator->Next(1, &moniker, nullptr);
            if (hr == S_FALSE){
                break;
            }
            check_hresult(hr, "Read DirectShow device moniker");
            result.devices.push_back(describe(*moniker.Get()));
        }
    }catch (const std::exception& error){
        result.devices.clear();
        result.error = error.what();
    }
    return result;
}

ComPtr<IMoniker> find_device_moniker(const DeviceKey& key){
    auto enumerator = enumerate();
    if (enumerator){
        while (true){
            ComPtr<IMoniker> moniker;
            HRESULT hr = enumerator->Next(1, &moniker, nullptr);
            if (hr == S_FALSE){
                break;
            }
            check_hresult(hr, "Resolve DirectShow device moniker");
            if (describe(*moniker.Get()).key == key){
                return moniker;
            }
        }
    }
    throw std::runtime_error("The selected DirectShow device is unavailable.");
}
}
#else
#include "DirectShowDevice.h"
namespace PokemonAutomation::DirectShow{
CatalogSnapshot enumerate_video_devices(){
    return {{}, "DirectShow capture is available only on Windows."};
}
}
#endif
