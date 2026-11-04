/*  DirectShow Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_DirectShowNative_H
#define PokemonAutomation_DirectShowNative_H

#ifdef _WIN32
#include <Windows.h>
#include <dshow.h>
#include <wrl/client.h>
#include <stdexcept>
#include <string>

namespace PokemonAutomation::DirectShow{

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;
void check_hresult(HRESULT result, const char* operation);

class ComApartment{
public:
    ComApartment(){
        check_hresult(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Initialize DirectShow COM apartment");
    }
    ~ComApartment(){ CoUninitialize(); }
    ComApartment(const ComApartment&) = delete;
    void operator=(const ComApartment&) = delete;
};

// Owns every component of an AM_MEDIA_TYPE, including pUnk, on every path.
class MediaType{
public:
    MediaType() = default;
    ~MediaType(){ clear(); }
    MediaType(const MediaType&) = delete;
    void operator=(const MediaType&) = delete;
    void clear() noexcept{
        CoTaskMemFree(value.pbFormat);
        if (value.pUnk){
            value.pUnk->Release();
        }
        value = {};
    }
    AM_MEDIA_TYPE value{};
};
class AllocatedMediaType{
public:
    ~AllocatedMediaType(){
        if (value){
            CoTaskMemFree(value->pbFormat);
            if (value->pUnk){
                value->pUnk->Release();
            }
            CoTaskMemFree(value);
        }
    }
    AllocatedMediaType() = default;
    AllocatedMediaType(const AllocatedMediaType&) = delete;
    void operator=(const AllocatedMediaType&) = delete;
    AM_MEDIA_TYPE* value = nullptr;
};

// COM objects are returned to their caller's apartment, never cached in UI data.
ComPtr<IMoniker> find_device_moniker(const struct DeviceKey& key);

}
#endif
#endif
