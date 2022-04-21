/*  Menu Detector
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_MenuDetector_H
#define PokemonAutomation_PokemonBDSP_MenuDetector_H

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/Inference/VisualDetector.h"
#include "CommonFramework/InferenceInfra/VisualInferenceCallback.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


class MenuDetector : public StaticScreenDetector{
public:
    MenuDetector(Color color = COLOR_RED);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const QImage& screen) const override;

private:
    Color m_color;
    ImageFloatBox m_line0;
    ImageFloatBox m_line1;
    ImageFloatBox m_line2;
    ImageFloatBox m_line3;
    ImageFloatBox m_line4;
    ImageFloatBox m_cross;
};


class MenuWatcher : public MenuDetector, public VisualInferenceCallback{
public:
    MenuWatcher(Color color = COLOR_RED);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool process_frame(const QImage& frame, WallClock timestamp) override;
};




}
}
}
#endif
