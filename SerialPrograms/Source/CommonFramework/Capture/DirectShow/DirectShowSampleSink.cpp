/*  DirectShow Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifdef _WIN32
#include <atomic>
#include <cstring>
#include <cwchar>
#include "DirectShowMediaType.h"
#include "DirectShowSampleSink.h"

namespace PokemonAutomation::DirectShow{
namespace{
HRESULT copy_type(AM_MEDIA_TYPE& target, const AM_MEDIA_TYPE& source) noexcept{
    target = source;
    target.pbFormat = nullptr;
    target.pUnk = nullptr;
    if (source.cbFormat){
        if (!source.pbFormat){
            target = {};
            return E_INVALIDARG;
        }
        target.pbFormat = static_cast<BYTE*>(CoTaskMemAlloc(source.cbFormat));
        if (!target.pbFormat){
            target = {};
            return E_OUTOFMEMORY;
        }
        memcpy(target.pbFormat, source.pbFormat, source.cbFormat);
    }
    target.pUnk = source.pUnk;
    if (target.pUnk){
        target.pUnk->AddRef();
    }
    return S_OK;
}

class PinEnumerator final : public IEnumPins{
public:
    explicit PinEnumerator(IPin* pin, bool consumed = false) : m_pin(pin), m_consumed(consumed){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override{
        if (!out){
            return E_POINTER;
        }
        *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_IEnumPins){
            return E_NOINTERFACE;
        }
        *out = static_cast<IEnumPins*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{ return ++m_refs; }
    ULONG STDMETHODCALLTYPE Release() override{
        ULONG refs = --m_refs;
        if (refs == 0){
            delete this;
        }
        return refs;
    }
    HRESULT STDMETHODCALLTYPE Next(ULONG count, IPin** pins, ULONG* fetched) override{
        if (!pins || (!fetched && count != 1)){
            return E_POINTER;
        }
        if (fetched){
            *fetched = 0;
        }
        if (count == 0){
            return S_OK;
        }
        if (m_consumed){
            return S_FALSE;
        }
        pins[0] = m_pin.Get();
        pins[0]->AddRef();
        m_consumed = true;
        if (fetched){
            *fetched = 1;
        }
        return count == 1 ? S_OK : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE Skip(ULONG count) override{
        if (count == 0){
            return S_OK;
        }
        bool available = !m_consumed;
        m_consumed = true;
        return available && count == 1 ? S_OK : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE Reset() override{
        m_consumed = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Clone(IEnumPins** out) override{
        if (!out){
            return E_POINTER;
        }
        *out = new (std::nothrow) PinEnumerator(m_pin.Get(), m_consumed);
        return *out ? S_OK : E_OUTOFMEMORY;
    }
private:
    std::atomic<ULONG> m_refs{1};
    ComPtr<IPin> m_pin;
    bool m_consumed;
};

class SampleSink final : public IBaseFilter, public IPin, public IMemInputPin{
public:
    SampleSink(const AM_MEDIA_TYPE& type, std::shared_ptr<SampleQueue> queue)
        : m_queue(std::move(queue))
    {
        if (!m_queue){
            throw std::invalid_argument("DirectShow sink requires owned sample storage.");
        }
        check_hresult(copy_type(m_type.value, type), "Copy DirectShow sink media type");
        if (auto video = video_layout(type)){
            m_video_bytes = video->bytes;
        }else if (auto audio = audio_layout(type)){
            m_audio_block = audio->block_bytes;
        }else{
            throw std::invalid_argument("Unsupported DirectShow sink media type.");
        }
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override{
        if (!out){
            return E_POINTER;
        }
        *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_IBaseFilter || iid == IID_IMediaFilter || iid == IID_IPersist){
            *out = static_cast<IBaseFilter*>(this);
        }else if (iid == IID_IPin){
            *out = static_cast<IPin*>(this);
        }else if (iid == IID_IMemInputPin){
            *out = static_cast<IMemInputPin*>(this);
        }else{
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{ return ++m_refs; }
    ULONG STDMETHODCALLTYPE Release() override{
        ULONG refs = --m_refs;
        if (refs == 0){
            delete this;
        }
        return refs;
    }
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID* out) override{
        if (!out){
            return E_POINTER;
        }
        *out = CLSID_NULL;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Stop() override{
        std::lock_guard<Mutex> lock(m_receive_lock);
        m_state.store(State_Stopped);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Pause() override{
        std::lock_guard<Mutex> lock(m_receive_lock);
        m_state.store(State_Paused);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Run(REFERENCE_TIME) override{
        std::lock_guard<Mutex> lock(m_receive_lock);
        m_state.store(State_Running);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetState(DWORD, FILTER_STATE* state) override{
        if (!state){
            return E_POINTER;
        }
        *state = m_state.load();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetSyncSource(IReferenceClock* clock) override{
        m_clock = clock;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetSyncSource(IReferenceClock** clock) override{
        return m_clock.CopyTo(clock);
    }
    HRESULT STDMETHODCALLTYPE EnumPins(IEnumPins** out) override{
        if (!out){
            return E_POINTER;
        }
        *out = new (std::nothrow) PinEnumerator(this);
        return *out ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE FindPin(LPCWSTR id, IPin** out) override{
        if (!out){
            return E_POINTER;
        }
        *out = nullptr;
        if (!id || wcscmp(id, L"Input") != 0){
            return VFW_E_NOT_FOUND;
        }
        *out = this;
        AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE QueryFilterInfo(FILTER_INFO* info) override{
        if (!info){
            return E_POINTER;
        }
        *info = {};
        wcscpy_s(info->achName, L"Pokemon Automation Sample Sink");
        info->pGraph = m_graph;
        if (m_graph){
            m_graph->AddRef();
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE JoinFilterGraph(IFilterGraph* graph, LPCWSTR) override{
        m_graph = graph;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE QueryVendorInfo(LPWSTR* out) override{
        if (out){
            *out = nullptr;
        }
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Connect(IPin*, const AM_MEDIA_TYPE*) override{ return E_UNEXPECTED; }
    HRESULT STDMETHODCALLTYPE ReceiveConnection(IPin* source, const AM_MEDIA_TYPE* type) override{
        if (!source || !type){
            return E_POINTER;
        }
        if (m_state.load() != State_Stopped){
            return VFW_E_NOT_STOPPED;
        }
        if (m_source){
            return VFW_E_ALREADY_CONNECTED;
        }
        PIN_DIRECTION direction;
        HRESULT hr = source->QueryDirection(&direction);
        if (FAILED(hr)){
            return hr;
        }
        if (direction != PINDIR_OUTPUT || QueryAccept(type) != S_OK){
            return VFW_E_TYPE_NOT_ACCEPTED;
        }
        m_source = source;
        m_connected.store(true);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Disconnect() override{
        if (m_state.load() != State_Stopped){
            return VFW_E_NOT_STOPPED;
        }
        m_connected.store(false);
        m_source.Reset();
        m_allocator.Reset();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ConnectedTo(IPin** source) override{
        if (!source){
            return E_POINTER;
        }
        *source = nullptr;
        return m_source ? m_source.CopyTo(source) : VFW_E_NOT_CONNECTED;
    }
    HRESULT STDMETHODCALLTYPE ConnectionMediaType(AM_MEDIA_TYPE* type) override{
        if (!type){
            return E_POINTER;
        }
        *type = {};
        return m_source ? copy_type(*type, m_type.value) : VFW_E_NOT_CONNECTED;
    }
    HRESULT STDMETHODCALLTYPE QueryPinInfo(PIN_INFO* info) override{
        if (!info){
            return E_POINTER;
        }
        *info = {};
        info->pFilter = this;
        AddRef();
        info->dir = PINDIR_INPUT;
        wcscpy_s(info->achName, L"Input");
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE QueryDirection(PIN_DIRECTION* direction) override{
        if (!direction){
            return E_POINTER;
        }
        *direction = PINDIR_INPUT;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE QueryId(LPWSTR* out) override{
        if (!out){
            return E_POINTER;
        }
        *out = static_cast<LPWSTR>(CoTaskMemAlloc(sizeof(L"Input")));
        if (!*out){
            return E_OUTOFMEMORY;
        }
        memcpy(*out, L"Input", sizeof(L"Input"));
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE* type) override{
        if (!type){
            return E_POINTER;
        }
        const auto& expected = m_type.value;
        return type->majortype == expected.majortype && type->subtype == expected.subtype &&
            type->formattype == expected.formattype && type->cbFormat == expected.cbFormat &&
            (type->cbFormat == 0 || (type->pbFormat && memcmp(type->pbFormat, expected.pbFormat, type->cbFormat) == 0))
            ? S_OK : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE EnumMediaTypes(IEnumMediaTypes** out) override{
        if (out){
            *out = nullptr;
        }
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE QueryInternalConnections(IPin**, ULONG*) override{ return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EndOfStream() override{ return S_OK; }
    HRESULT STDMETHODCALLTYPE BeginFlush() override{
        std::lock_guard<Mutex> lock(m_receive_lock);
        m_flushing.store(true);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE EndFlush() override{
        std::lock_guard<Mutex> lock(m_receive_lock);
        m_flushing.store(false);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE NewSegment(REFERENCE_TIME, REFERENCE_TIME, double rate) override{
        std::lock_guard<Mutex> lock(m_receive_lock);
        return rate == 1.0 ? S_OK : E_INVALIDARG;
    }
    HRESULT STDMETHODCALLTYPE GetAllocator(IMemAllocator** out) override{
        if (!out){
            return E_POINTER;
        }
        *out = nullptr;
        if (!m_allocator){
            HRESULT hr = CoCreateInstance(CLSID_MemoryAllocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_allocator));
            if (FAILED(hr)){
                return hr;
            }
        }
        return m_allocator.CopyTo(out);
    }
    HRESULT STDMETHODCALLTYPE NotifyAllocator(IMemAllocator* allocator, BOOL) override{
        if (!allocator){
            return E_POINTER;
        }
        m_allocator = allocator;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetAllocatorRequirements(ALLOCATOR_PROPERTIES*) override{ return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Receive(IMediaSample* sample) override{
        std::unique_lock<Mutex> lock(m_receive_lock, std::try_to_lock);
        if (!lock){
            return S_OK;
        }
        if (!sample){
            return E_POINTER;
        }
        if (!m_connected.load()){
            return VFW_E_NOT_CONNECTED;
        }
        if (m_flushing.load()){
            return S_FALSE;
        }
        if (m_state.load() == State_Stopped){
            return VFW_E_WRONG_STATE;
        }
        if (m_state.load() == State_Paused){
            return S_OK;
        }
        AllocatedMediaType changed;
        HRESULT hr = sample->GetMediaType(&changed.value);
        if (FAILED(hr)){
            return hr;
        }
        if (hr == S_OK && changed.value && QueryAccept(changed.value) != S_OK){
            return VFW_E_INVALIDMEDIATYPE;
        }
        BYTE* data = nullptr;
        hr = sample->GetPointer(&data);
        if (FAILED(hr)){
            return hr;
        }
        const long length = sample->GetActualDataLength();
        if (length <= 0 || length > sample->GetSize() ||
            (m_video_bytes != 0 && static_cast<size_t>(length) != m_video_bytes) ||
            (m_audio_block != 0 && static_cast<size_t>(length) % m_audio_block != 0)){
            return VFW_E_BUFFER_OVERFLOW;
        }
        REFERENCE_TIME start = 0;
        REFERENCE_TIME end = 0;
        sample->GetTime(&start, &end);
        m_queue->push(data, static_cast<size_t>(length), start, end, sample->IsDiscontinuity() == S_OK);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ReceiveMultiple(IMediaSample** samples, long count, long* processed) override{
        if (!samples || !processed){
            return E_POINTER;
        }
        *processed = 0;
        if (count < 0){
            return E_INVALIDARG;
        }
        for (long i = 0; i < count; i++){
            HRESULT hr = Receive(samples[i]);
            if (hr != S_OK){
                return hr;
            }
            (*processed)++;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ReceiveCanBlock() override{ return S_FALSE; }
private:
    std::atomic<ULONG> m_refs{1};
    std::atomic<FILTER_STATE> m_state{State_Stopped};
    std::atomic<bool> m_flushing{false};
    std::atomic<bool> m_connected{false};
    Mutex m_receive_lock;
    std::shared_ptr<SampleQueue> m_queue;
    size_t m_video_bytes = 0;
    size_t m_audio_block = 0;
    MediaType m_type;
    ComPtr<IPin> m_source;
    ComPtr<IReferenceClock> m_clock;
    ComPtr<IMemAllocator> m_allocator;
    IFilterGraph* m_graph = nullptr;
};
}

ComPtr<IBaseFilter> make_sample_sink(const AM_MEDIA_TYPE& type, std::shared_ptr<SampleQueue> queue){
    ComPtr<IBaseFilter> sink;
    sink.Attach(new SampleSink(type, std::move(queue)));
    return sink;
}
}
#endif
