/*  Beam Setter
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 *
 *      Drop a wishing piece and determine if it is red or purple.
 *
 */

#ifndef PokemonAutomation_PokemonSwSh_BeamSetter_H
#define PokemonAutomation_PokemonSwSh_BeamSetter_H

#include <vector>
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "CommonFramework/Tools/ConsoleHandle.h"
#include "CommonFramework/Tools/VideoFeed.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{


class BeamSetter{
public:
    enum Detection{
        NO_DETECTION,
        RED_DETECTED,
        RED_ASSUMED,
        PURPLE,
    };

public:
    BeamSetter(ProgramEnvironment& env, BotBaseContext& context, ConsoleHandle& console);

    Detection run(
        bool save_screenshot,
        uint16_t timeout_ticks,
        double min_brightness,
        double min_euclidean,
        double min_delta_ratio,
        double min_sigma_ratio
    );


private:
    ProgramEnvironment& m_env;
    BotBaseContext& m_context;
    ConsoleHandle& m_console;
    InferenceBoxScope m_text_box;
    InferenceBoxScope m_box;
    std::vector<ImageFloatBox> m_boxes;
};




}
}
}
#endif
