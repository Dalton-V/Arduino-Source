/*  DirectShow OBS Capture Probe
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

// Explicit opt-in hardware diagnostic; never part of the unattended test suite.
#include <chrono>
#include <iostream>
#include <thread>
#include "CommonFramework/Capture/DirectShow/DirectShowDevice.h"
#include "CommonFramework/Capture/DirectShow/DirectShowMediaType.h"
#include "CommonFramework/Capture/DirectShow/DirectShowSampleSink.h"

namespace{
using namespace PokemonAutomation::DirectShow;

// The queue outlives all graph references, including on startup exceptions.
struct ProbeGraph{
    ProbeGraph(size_t bytes) : queue(std::make_shared<SampleQueue>(4, bytes, true)){}
    ~ProbeGraph(){
        if (control){
            control->Stop();
        }
        if (graph){
            if (output){
                graph->Disconnect(output.Get());
            }
            if (input){
                graph->Disconnect(input.Get());
            }
            if (sink){
                graph->RemoveFilter(sink.Get());
            }
            if (source){
                graph->RemoveFilter(source.Get());
            }
        }
    }
    std::shared_ptr<SampleQueue> queue;
    ComPtr<IGraphBuilder> graph;
    ComPtr<IBaseFilter> source;
    ComPtr<IBaseFilter> sink;
    ComPtr<IPin> output;
    ComPtr<IPin> input;
    ComPtr<IMediaControl> control;
};

bool capture(IBaseFilter* source, IPin* pin, AM_MEDIA_TYPE& type, const VideoLayout& layout){
    ProbeGraph capture(layout.bytes);
    capture.source = source;
    capture.output = pin;
    ComPtr<IAMStreamConfig> config;
    if (SUCCEEDED(pin->QueryInterface(IID_PPV_ARGS(&config)))){
        check_hresult(config->SetFormat(&type), "Select OBS stream format");
    }
    capture.sink = make_sample_sink(type, capture.queue);
    check_hresult(CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&capture.graph)), "Create graph");
    check_hresult(capture.graph->AddFilter(source, L"OBS"), "Add OBS source");
    check_hresult(capture.graph->AddFilter(capture.sink.Get(), L"Samples"), "Add native sink");
    check_hresult(capture.sink.As(&capture.input), "Get sink pin");
    check_hresult(capture.graph->ConnectDirect(pin, capture.input.Get(), &type), "Connect OBS");
    check_hresult(capture.graph.As(&capture.control), "Get graph control");
    check_hresult(capture.control->Run(), "Start OBS capture");
    size_t frames = 0;
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < until){
        if (auto sample = capture.queue->pop()){
            if (sample->size != layout.bytes){
                throw std::runtime_error("OBS delivered an invalid sample size.");
            }
            frames++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check_hresult(capture.control->Stop(), "Stop OBS capture");
    std::cout << "Captured " << frames << " frames at " << layout.width << "x" << layout.height
        << ", native interval " << layout.interval << " (100 ns)\n";
    return frames > 0;
}

int probe(){
    auto catalog = enumerate_video_devices();
    if (!catalog.error.empty()){
        throw std::runtime_error(catalog.error);
    }
    ComApartment apartment;
    for (const auto& device : catalog.devices){
        // Opening other physical cards is deliberately excluded from this probe.
        if (device.name != "OBS Virtual Camera"){
            continue;
        }
        auto moniker = find_device_moniker(device.key);
        ComPtr<IBaseFilter> source;
        check_hresult(moniker->BindToObject(nullptr, nullptr, IID_PPV_ARGS(&source)), "Open OBS");
        ComPtr<IEnumPins> pins;
        check_hresult(source->EnumPins(&pins), "Enumerate OBS pins");
        while (true){
            ComPtr<IPin> pin;
            HRESULT hr = pins->Next(1, &pin, nullptr);
            if (hr == S_FALSE){
                break;
            }
            check_hresult(hr, "Read OBS pin");
            PIN_DIRECTION direction;
            check_hresult(pin->QueryDirection(&direction), "Read OBS pin direction");
            if (direction != PINDIR_OUTPUT){
                continue;
            }
            ComPtr<IEnumMediaTypes> types;
            check_hresult(pin->EnumMediaTypes(&types), "Enumerate OBS media types");
            while (true){
                AllocatedMediaType type;
                hr = types->Next(1, &type.value, nullptr);
                if (hr == S_FALSE){
                    break;
                }
                check_hresult(hr, "Read OBS media type");
                if (auto layout = video_layout(*type.value)){
                    return capture(source.Get(), pin.Get(), *type.value, *layout) ? 0 : 2;
                }
            }
        }
        throw std::runtime_error("OBS has no supported native video format.");
    }
    throw std::runtime_error("OBS Virtual Camera is unavailable.");
}
}

int main(){
    try{
        return probe();
    }catch (const std::exception& error){
        std::cerr << error.what() << "\n";
        return 1;
    }
}
