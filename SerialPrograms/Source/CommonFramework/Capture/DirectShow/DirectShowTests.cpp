/*  DirectShow Capture Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include <stdexcept>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "DirectShowDevice.h"
#include "DirectShowSampleQueue.h"
#ifdef _WIN32
#include "DirectShowMediaType.h"
#include "DirectShowSampleSink.h"
#include "CommonFramework/VideoPipeline/Backends/DirectShowFrameQt.h"
#include <dvdmedia.h>
#include <mmreg.h>
#endif

namespace PokemonAutomation{
namespace{
using namespace DirectShow;

class IdentityTest : public UnitTest{
public:
    IdentityTest() : UnitTest("DirectShow - Stable endpoint identities"){}
    UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::array<std::string, 5> keys{
            "capture:0:1", "@device:sw:{123}\\camera", "USB capture Ã¦â€”Â¥Ã¦Å“Â¬Ã¨ÂªÅ¾", "same name / different device", std::string(65536, 'x')
        };
        for (const auto& key : keys){
            for (auto kind : {DeviceKeyKind::DevicePath, DeviceKeyKind::Moniker}){
                for (auto stream : {EndpointKind::Video, EndpointKind::EmbeddedAudio}){
                    Endpoint endpoint{{kind, key}, stream, "pin:0:Ã¦â€”Â¥Ã¦Å“Â¬Ã¨ÂªÅ¾"};
                    auto parsed = parse_endpoint(serialize_endpoint(endpoint));
                    if (!parsed || *parsed != endpoint){
                        return "Endpoint identity did not round trip.";
                    }
                }
            }
        }
        const std::array<std::string, 12> malformed{
            "", "dshow:0:Camera", "pa-dshow:2:path:video:1:x0:", "pa-dshow:1:path:other:1:x0:",
            "pa-dshow:1:path:video:0:0:", "pa-dshow:1:path:video:-1:x0:",
            "pa-dshow:1:path:video:01:x0:", "pa-dshow:1:path:video:65537:x0:",
            "pa-dshow:1:path:video:2:x0:", "pa-dshow:1:path:video:1:x0:trailing",
            "pa-dshow:1:path:video:1:x-1:", "pa-dshow:1:path:video:999999999999:x0:"
        };
        for (const auto& input : malformed){
            if (parse_endpoint(input)){
                return "Malformed endpoint accepted: " + input;
            }
        }
        // Every truncation of a valid descriptor is invalid, including a
        // completed first field with an absent pin field.
        const std::string encoded = serialize_endpoint({{DeviceKeyKind::Moniker, "device"}, EndpointKind::Video, "pin"});
        for (size_t i = 0; i < encoded.size(); i++){
            if (parse_endpoint(std::string_view(encoded).substr(0, i))){
                return "Truncated endpoint accepted.";
            }
        }
        try{
            serialize_endpoint({{DeviceKeyKind::DevicePath, std::string("a\0b", 3)}, EndpointKind::Video, ""});
            return "Embedded NUL accepted.";
        }catch (const std::invalid_argument&){}
        return true;
    }
};

class QueueTest : public UnitTest{
public:
    QueueTest() : UnitTest("DirectShow - Bounded sample ownership"){}
    UnitTestResult run(Logger&, CancellableScope&) const override{
        const unsigned char first[] = {1, 2, 3, 4};
        const unsigned char second[] = {5, 6, 7, 8};
        for (size_t iteration = 0; iteration < 500; iteration++){
            SampleQueue queue(2, 4, false);
            if (!queue.push(first, 4, 1, 2, false) || !queue.push(second, 4, 2, 3, true)){
                return "Queue rejected available storage.";
            }
            auto a = queue.pop();
            auto b = queue.pop();
            if (!a || !b || a->bytes[0] != 1 || b->bytes[0] != 5 || !b->discontinuity || a->start != 1){
                return "FIFO content or metadata changed.";
            }
            if (queue.push(first, 4, 3, 4, false) || queue.dropped() != 1){
                return "Retained buffers did not bound allocation.";
            }
            a.reset();
            if (!queue.push(first, 4, 3, 4, false)){
                return "Released buffer did not become reusable.";
            }
            queue.close();
            if (queue.pop() || queue.push(first, 4, 4, 5, false) || b->bytes[0] != 5){
                return "Close permitted publication or invalidated a reader.";
            }
        }
        SampleQueue latest(2, 4, true);
        latest.push(first, 4, 1, 2, false);
        latest.push(second, 4, 2, 3, false);
        auto frame = latest.pop();
        if (!frame || frame->bytes[0] != 5 || latest.pop()){
            return "Preview queue did not coalesce frames.";
        }
        if (latest.push(first, 5, 0, 0, false) || latest.push(nullptr, 1, 0, 0, false)){
            return "Invalid sample accepted.";
        }
        return true;
    }
};

#ifdef _WIN32
class SinkInterfaceTest : public UnitTest{
public:
    SinkInterfaceTest() : UnitTest("DirectShow - Native sink COM ownership"){}
    UnitTestResult run(Logger&, CancellableScope&) const override{
        ComApartment apartment;
        VIDEOINFOHEADER header{};
        header.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        header.bmiHeader.biPlanes = 1;
        header.bmiHeader.biWidth = 4;
        header.bmiHeader.biHeight = 2;
        header.bmiHeader.biCompression = BI_RGB;
        header.bmiHeader.biBitCount = 32;
        header.AvgTimePerFrame = 333333;
        AM_MEDIA_TYPE type{};
        type.majortype = MEDIATYPE_Video;
        type.subtype = MEDIASUBTYPE_RGB32;
        type.formattype = FORMAT_VideoInfo;
        type.pbFormat = reinterpret_cast<BYTE*>(&header);
        type.cbFormat = sizeof(header);
        auto queue = std::make_shared<SampleQueue>(2, 32, true);
        for (size_t cycle = 0; cycle < 500; cycle++){
            auto sink = make_sample_sink(type, queue);
            ComPtr<IEnumPins> pins;
            check_hresult(sink->EnumPins(&pins), "Enumerate native sink pins");
            ComPtr<IEnumPins> clone;
            check_hresult(pins->Clone(&clone), "Clone native sink enumerator");
            ComPtr<IPin> pin;
            if (pins->Next(1, &pin, nullptr) != S_OK || pins->Skip(1) != S_FALSE){
                return "Native sink pin enumeration failed.";
            }
            ComPtr<IPin> cloned_pin;
            if (clone->Next(1, &cloned_pin, nullptr) != S_OK || cloned_pin.Get() != pin.Get()){
                return "Pin enumeration did not retain stable identity.";
            }
            ComPtr<IMemInputPin> input;
            check_hresult(pin.As(&input), "Get native sink allocator interface");
            ComPtr<IMemAllocator> allocator;
            check_hresult(input->GetAllocator(&allocator), "Get native sink default allocator");
            check_hresult(sink->Run(0), "Run native sink");
            if (input->Receive(nullptr) != E_POINTER || pin->QueryAccept(&type) != S_OK){
                return "Native sink input validation failed.";
            }
            check_hresult(pin->BeginFlush(), "Flush native sink");
            check_hresult(pin->EndFlush(), "End native sink flush");
            check_hresult(sink->Stop(), "Stop native sink");
            // Enumerators and pins own the filter, so dropping the public
            // filter handle cannot invalidate those COM references.
            sink.Reset();
            PIN_DIRECTION direction;
            check_hresult(pin->QueryDirection(&direction), "Read retained native sink pin");
            if (direction != PINDIR_INPUT){
                return "Retained sink pin changed direction.";
            }
        }
        std::weak_ptr<SampleQueue> retained = queue;
        auto retained_sink = make_sample_sink(type, queue);
        queue.reset();
        if (retained.expired()){
            return "Sink did not retain its callback storage.";
        }
        retained_sink.Reset();
        if (!retained.expired()){
            return "Sink callback storage leaked after the last COM reference.";
        }
        return true;
    }
};

class FrameBridgeTest : public UnitTest{
public:
    FrameBridgeTest() : UnitTest("DirectShow - Frame pixels and ownership"){}
    UnitTestResult run(Logger&, CancellableScope&) const override{
        auto sample = std::make_shared<SampleBuffer>();
        sample->bytes = {255,0,0, 255,255,255, 0,0, 0,0,255, 0,255,0, 0,0};
        sample->size = sample->bytes.size();
        VideoLayout layout{PixelFormat::BGR24, 2, 2, 8, 16, true, 166833};
        QVideoFrame frame = make_directshow_frame(sample, layout);
        sample.reset();
        QImage image = frame.toImage();
        if (image.size() != QSize(2,2) || image.pixelColor(0,0) != QColor(Qt::red) ||
            image.pixelColor(1,0) != QColor(Qt::green) || image.pixelColor(0,1) != QColor(Qt::blue) ||
            image.pixelColor(1,1) != QColor(Qt::white)){
            return "RGB row padding, orientation, or pixel ownership was lost.";
        }
        auto yuv = std::make_shared<SampleBuffer>();
        yuv->bytes = {235,235,235,235,128,128};
        yuv->size = 6;
        layout = {PixelFormat::NV12, 2, 2, 2, 6, false, 166666};
        QVideoFrame nv12 = make_directshow_frame(yuv, layout);
        if (!nv12.map(QVideoFrame::ReadOnly)){
            return "Native NV12 frame could not be mapped.";
        }
        bool planes = nv12.planeCount() == 2 && nv12.bytesPerLine(0) == 2 && nv12.bytesPerLine(1) == 2 &&
            nv12.bits(1)[0] == 128 && nv12.bits(0)[0] == 235;
        nv12.unmap();
        if (!planes || nv12.toImage().isNull()){
            return "NV12 planes or snapshot conversion failed.";
        }
        yuv->size = 5;
        try{
            make_directshow_frame(yuv, layout);
            return "Truncated frame payload was accepted.";
        }catch (const std::runtime_error&){}
        return true;
    }
};

class MediaTypeTest : public UnitTest{
public:
    MediaTypeTest() : UnitTest("DirectShow - Media type validation"){}
    UnitTestResult run(Logger&, CancellableScope&) const override{
        VIDEOINFOHEADER header{};
        header.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        header.bmiHeader.biPlanes = 1;
        header.bmiHeader.biWidth = 1920;
        header.bmiHeader.biHeight = 1080;
        header.bmiHeader.biCompression = MAKEFOURCC('N','V','1','2');
        header.bmiHeader.biBitCount = 12;
        header.AvgTimePerFrame = 166833;
        AM_MEDIA_TYPE type{};
        type.majortype = MEDIATYPE_Video;
        type.subtype = MEDIASUBTYPE_NV12;
        type.formattype = FORMAT_VideoInfo;
        type.pbFormat = reinterpret_cast<BYTE*>(&header);
        type.cbFormat = sizeof(header);
        auto layout = video_layout(type);
        if (!layout || layout->bytes != 1920 * 1080 * 3 / 2 || layout->interval != 166833 || layout->bottom_up){
            return "NV12 layout or fractional frame interval changed.";
        }
        for (size_t length = 0; length < sizeof(header); length++){
            type.cbFormat = static_cast<ULONG>(length);
            if (video_layout(type)){
                return "Truncated video header accepted.";
            }
        }
        type.cbFormat = sizeof(header);
        header.bmiHeader.biWidth = 1919;
        if (video_layout(type)){
            return "Odd NV12 width accepted.";
        }
        header.bmiHeader.biWidth = 1920;
        header.bmiHeader.biSizeImage = 1;
        if (video_layout(type)){
            return "Ambiguous sample stride accepted.";
        }
        header.bmiHeader.biSizeImage = 0;
        header.bmiHeader.biWidth = 0x7fffffff;
        if (video_layout(type)){
            return "Unbounded dimensions accepted.";
        }
        WAVEFORMATEX wave{};
        wave.wFormatTag = WAVE_FORMAT_PCM;
        wave.nChannels = 2;
        wave.nSamplesPerSec = 48000;
        wave.wBitsPerSample = 16;
        wave.nBlockAlign = 4;
        wave.nAvgBytesPerSec = 192000;
        type.majortype = MEDIATYPE_Audio;
        type.subtype = MEDIASUBTYPE_PCM;
        type.formattype = FORMAT_WaveFormatEx;
        type.pbFormat = reinterpret_cast<BYTE*>(&wave);
        type.cbFormat = sizeof(wave);
        auto audio = audio_layout(type);
        if (!audio || audio->rate != 48000 || audio->channels != 2 || audio->block_bytes != 4){
            return "Native PCM format was not preserved.";
        }
        wave.nBlockAlign = 2;
        if (audio_layout(type)){
            return "Mismatched PCM block alignment accepted.";
        }
        wave.nBlockAlign = 4;
        wave.cbSize = 65535;
        if (audio_layout(type)){
            return "Truncated audio extension accepted.";
        }
        return true;
    }
};
#endif
}

void add_tests_DirectShow(UnitTestDatabase& database){
    database.add<IdentityTest>();
    database.add<QueueTest>();
#ifdef _WIN32
    database.add<MediaTypeTest>();
    database.add<FrameBridgeTest>();
    database.add<SinkInterfaceTest>();
#endif
}
}
