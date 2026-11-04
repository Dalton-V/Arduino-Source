/*  DirectShow Video Source
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifdef _WIN32
#include <QAbstractVideoBuffer>
#include "DirectShowFrameQt.h"

namespace PokemonAutomation{
namespace{
class NativeBuffer final : public QAbstractVideoBuffer{
public:
    NativeBuffer(std::shared_ptr<const DirectShow::SampleBuffer> sample, const DirectShow::VideoLayout& layout)
        : m_sample(std::move(sample)), m_layout(layout)
    {
        using DirectShow::PixelFormat;
        QVideoFrameFormat::PixelFormat pixel;
        if (layout.format == PixelFormat::NV12){
            pixel = QVideoFrameFormat::Format_NV12;
        }else if (layout.format == PixelFormat::YUY2){
            pixel = QVideoFrameFormat::Format_YUYV;
        }else{
            pixel = QVideoFrameFormat::pixelFormatFromImageFormat(QImage::Format_RGB32);
        }
        m_format = QVideoFrameFormat(QSize(static_cast<int>(layout.width), static_cast<int>(layout.height)), pixel);
        m_format.setStreamFrameRate(10000000.0 / static_cast<double>(layout.interval));
        if (layout.format == PixelFormat::BGR24 || layout.bottom_up){
            // Normalize RGB rows once on the delivery task. The raw slot stays
            // retained, so expanded frame allocations remain bounded by slots.
            m_rgb.resize(layout.width * layout.height * 4);
            for (size_t y = 0; y < layout.height; y++){
                size_t source_y = layout.bottom_up ? layout.height - 1 - y : y;
                const unsigned char* row = m_sample->bytes.data() + source_y * layout.stride;
                unsigned char* out = m_rgb.data() + y * layout.width * 4;
                const size_t step = layout.format == PixelFormat::BGR24 ? 3 : 4;
                for (size_t x = 0; x < layout.width; x++){
                    memcpy(out + x * 4, row + x * step, 3);
                    out[x * 4 + 3] = 255;
                }
            }
        }
    }
    QVideoFrameFormat format() const override{ return m_format; }
    MapData map(QVideoFrame::MapMode mode) override{
        if (mode != QVideoFrame::ReadOnly){
            return {};
        }
        MapData result;
        result.planeCount = 1;
        result.data[0] = m_rgb.empty() ? const_cast<uchar*>(m_sample->bytes.data()) : m_rgb.data();
        result.bytesPerLine[0] = static_cast<int>(m_rgb.empty() ? m_layout.stride : m_layout.width * 4);
        result.dataSize[0] = static_cast<int>(m_rgb.empty() ? m_layout.bytes : m_rgb.size());
        if (m_layout.format == DirectShow::PixelFormat::NV12){
            result.planeCount = 2;
            const size_t luma = m_layout.stride * m_layout.height;
            result.dataSize[0] = static_cast<int>(luma);
            result.data[1] = result.data[0] + luma;
            result.bytesPerLine[1] = static_cast<int>(m_layout.stride);
            result.dataSize[1] = static_cast<int>(m_layout.bytes - luma);
        }
        return result;
    }
private:
    std::shared_ptr<const DirectShow::SampleBuffer> m_sample;
    DirectShow::VideoLayout m_layout;
    QVideoFrameFormat m_format;
    std::vector<unsigned char> m_rgb;
};
}
QVideoFrame make_directshow_frame(std::shared_ptr<const DirectShow::SampleBuffer> sample, const DirectShow::VideoLayout& layout){
    if (!sample || sample->size != layout.bytes || sample->bytes.size() < layout.bytes){
        throw std::runtime_error("Invalid DirectShow frame payload.");
    }
    return QVideoFrame(std::make_unique<NativeBuffer>(std::move(sample), layout));
}
}
#endif
