/*  DirectShow Video Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifdef _WIN32
#include <algorithm>
#include <cmath>
#include <limits>
#include "Common/Cpp/Strings/Unicode.h"
#include "DirectShowVideoCapture.h"
#include "DirectShowSampleSink.h"

namespace PokemonAutomation::DirectShow{
namespace{
struct Candidate{
    VideoMode mode;
    ComPtr<IPin> pin;
    std::unique_ptr<AllocatedMediaType> type;
};
std::string pin_id(IPin& pin){
    LPWSTR id = nullptr;
    check_hresult(pin.QueryId(&id), "Read DirectShow pin identifier");
    std::string result;
    try{
        if (id){
            std::u16string text;
            for (const wchar_t* p = id; *p; ++p){
                text.push_back(static_cast<char16_t>(*p));
            }
            result = utr16_to_str(text);
        }
    }catch (...){
        CoTaskMemFree(id);
        throw;
    }
    CoTaskMemFree(id);
    return result;
}
std::vector<Candidate> candidates(IBaseFilter& source){
    std::vector<Candidate> result;
    ComPtr<IEnumPins> pins;
    check_hresult(source.EnumPins(&pins), "Enumerate DirectShow video pins");
    while (true){
        ComPtr<IPin> pin;
        const HRESULT next = pins->Next(1, &pin, nullptr);
        if (next == S_FALSE){
            break;
        }
        check_hresult(next, "Read DirectShow video pin");
        PIN_DIRECTION direction;
        check_hresult(pin->QueryDirection(&direction), "Read DirectShow pin direction");
        if (direction != PINDIR_OUTPUT){
            continue;
        }
        const std::string id = pin_id(*pin.Get());
        auto append = [&](std::unique_ptr<AllocatedMediaType> type){
            if (type->value){
                if (auto layout = video_layout(*type->value)){
                    result.push_back({{*layout, id}, pin, std::move(type)});
                }
            }
        };
        ComPtr<IAMStreamConfig> config;
        if (SUCCEEDED(pin.As(&config))){
            int count = 0;
            int size = 0;
            check_hresult(config->GetNumberOfCapabilities(&count, &size), "Query DirectShow video capabilities");
            if (count < 0 || count > 4096 || size <= 0 || size > 1024 * 1024){
                throw std::runtime_error("Invalid DirectShow video capability buffer size.");
            }
            std::vector<BYTE> buffer(static_cast<size_t>(size));
            for (int i = 0; i < count; i++){
                auto type = std::make_unique<AllocatedMediaType>();
                check_hresult(config->GetStreamCaps(i, &type->value, buffer.data()), "Read DirectShow video capability");
                append(std::move(type));
            }
        }else{
            ComPtr<IEnumMediaTypes> types;
            check_hresult(pin->EnumMediaTypes(&types), "Enumerate DirectShow video media types");
            for (size_t i = 0; ; i++){
                auto type = std::make_unique<AllocatedMediaType>();
                HRESULT hr = types->Next(1, &type->value, nullptr);
                if (hr == S_FALSE){
                    break;
                }
                check_hresult(hr, "Read DirectShow video media type");
                if (i >= 4096){
                    throw std::runtime_error("Too many DirectShow media types.");
                }
                append(std::move(type));
            }
        }
    }
    return result;
}
}

size_t fps_label(int64_t interval){
    if (interval <= 0){
        return 0;
    }
    return static_cast<size_t>(std::llround(10000000.0 / static_cast<double>(interval)));
}

struct VideoCapture::Data{
    ComApartment apartment;
    // Sample memory outlives every graph and sink reference.
    std::shared_ptr<SampleQueue> queue;
    ComPtr<IGraphBuilder> graph;
    ComPtr<IBaseFilter> source;
    ComPtr<IBaseFilter> sink;
    ComPtr<IPin> output;
    ComPtr<IPin> input;
    ComPtr<IMediaControl> control;
    ComPtr<IMediaEvent> events;
    VideoMode selected;
    std::vector<VideoMode> supported;
    ~Data(){
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
};

VideoCapture::VideoCapture(const DeviceKey& key, const VideoRequest& request)
    : m_data(std::make_unique<Data>())
{
    Data& data = *m_data;
    auto moniker = find_device_moniker(key);
    check_hresult(moniker->BindToObject(nullptr, nullptr, IID_PPV_ARGS(&data.source)), "Open DirectShow camera");
    auto choices = candidates(*data.source.Get());
    if (choices.empty()){
        throw std::runtime_error("This DirectShow camera has no supported uncompressed SDR video formats.");
    }
    Candidate* selected = nullptr;
    int best = (std::numeric_limits<int>::min)();
    for (auto& candidate : choices){
        const auto& layout = candidate.mode.layout;
        data.supported.push_back(candidate.mode);
        if (request.format && layout.format != *request.format){
            continue;
        }
        if (request.fps && fps_label(layout.interval) != request.fps){
            continue;
        }
        int score = layout.format == PixelFormat::NV12 ? 4 : layout.format == PixelFormat::YUY2 ? 3 : 1;
        if (layout.width == request.width && layout.height == request.height){
            score += 100;
        }
        if (score > best){
            selected = &candidate;
            best = score;
        }
    }
    if (!selected){
        throw std::runtime_error("The requested DirectShow pixel format or frame rate is unavailable.");
    }
    data.selected = selected->mode;
    data.output = selected->pin;
    AM_MEDIA_TYPE& type = *selected->type->value;
    ComPtr<IAMStreamConfig> config;
    if (SUCCEEDED(data.output.As(&config))){
        check_hresult(config->SetFormat(&type), "Select DirectShow stream format");
    }
    data.queue = std::make_shared<SampleQueue>(4, data.selected.layout.bytes, true);
    data.sink = make_sample_sink(type, data.queue);
    check_hresult(CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&data.graph)), "Create DirectShow capture graph");
    check_hresult(data.graph->AddFilter(data.source.Get(), L"Capture Device"), "Add DirectShow camera");
    check_hresult(data.graph->AddFilter(data.sink.Get(), L"Video Samples"), "Add DirectShow video sink");
    check_hresult(data.sink.As(&data.input), "Get DirectShow sink input");
    check_hresult(data.graph->ConnectDirect(data.output.Get(), data.input.Get(), &type), "Connect DirectShow video");
    check_hresult(data.graph.As(&data.control), "Get DirectShow graph control");
    check_hresult(data.graph.As(&data.events), "Get DirectShow graph events");
}
VideoCapture::~VideoCapture() = default;
const VideoMode& VideoCapture::mode() const{ return m_data->selected; }
const std::vector<VideoMode>& VideoCapture::modes() const{ return m_data->supported; }
void VideoCapture::start(){
    check_hresult(m_data->control->Run(), "Start DirectShow video capture");
}
std::shared_ptr<const SampleBuffer> VideoCapture::pop(){ return m_data->queue->pop(); }
void VideoCapture::check_events(){
    long code = 0;
    LONG_PTR first = 0;
    LONG_PTR second = 0;
    while (m_data->events->GetEvent(&code, &first, &second, 0) == S_OK){
        m_data->events->FreeEventParams(code, first, second);
        if (code == EC_ERRORABORT || code == EC_DEVICE_LOST || code == EC_COMPLETE){
            throw std::runtime_error("DirectShow video stopped or the device was disconnected. Use Reset Video to reconnect.");
        }
    }
}
}
#endif
