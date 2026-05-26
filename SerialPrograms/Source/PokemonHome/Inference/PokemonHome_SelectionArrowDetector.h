/*  Selection Arrow Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonHome_SelectionArrowDetector_H
#define PokemonAutomation_PokemonHome_SelectionArrowDetector_H

#include <optional>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


enum class SelectionArrowType{
    RIGHT,
    DOWN,
};

const int RIGHT_ARROW_POSITION_COUNT = 3;
enum class SelectionArrowRightPosition{
    BOX_NAME,
    BOX_SPACES,
    NEWEST_30
};

class SelectionArrowDetector : public StaticScreenDetector{
public:
    SelectionArrowDetector(
        Color color,
        VideoOverlay* overlay,
        SelectionArrowType type,
        const ImageFloatBox& box
    );

    static ImageFloatBox arrow_box_for_position(SelectionArrowRightPosition position);

    const ImageFloatBox& last_detected() const{ return m_last_detected; }

    virtual void make_overlays(VideoOverlaySet& items) const override;

    virtual bool detect(const ImageViewRGB32& screen) override;

    std::vector<ImageFloatBox> down_arrow_positions() const{ return m_down_arrow_positions; }
    std::vector<ImageFloatBox> right_arrow_positions() const{ return m_right_arrow_positions; }

private:
    friend class SelectionArrowWatcher;

    const Color m_color;
    VideoOverlay* m_overlay;
    const SelectionArrowType m_type;
    const ImageFloatBox m_arrow_box;

    ImageFloatBox m_last_detected;
    std::optional<OverlayBoxScope> m_last_detected_box;

    std::vector<ImageFloatBox> m_down_arrow_positions;
    std::vector<ImageFloatBox> m_right_arrow_positions;
};

class SelectionArrowWatcher : public DetectorToFinder<SelectionArrowDetector>{
public:
    SelectionArrowWatcher(
        Color color,
        VideoOverlay* overlay,
        SelectionArrowType type,
        const ImageFloatBox& box,
        std::chrono::milliseconds hold_duration = std::chrono::milliseconds(250)
    )
         : DetectorToFinder("SelectionArrowWatcher", hold_duration, color, overlay, type, box)
    {}
};


}
}
}
#endif
