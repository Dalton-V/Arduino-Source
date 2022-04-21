/*  Outbreak Reader
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonLA_OutbreakReader_H
#define PokemonAutomation_PokemonLA_OutbreakReader_H

#include "CommonFramework/Language.h"
#include "CommonFramework/Logging/LoggerQt.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "CommonFramework/OCR/OCR_StringMatchResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLA{


class OutbreakReader{
public:
    OutbreakReader(LoggerQt& logger, Language language, VideoOverlay& overlay);

    OCR::StringMatchResult read(const QImage& screen) const;


private:
    LoggerQt& m_logger;
    Language m_language;
    InferenceBoxScope m_dialog_box0;
    InferenceBoxScope m_dialog_box1;
    InferenceBoxScope m_text_box;
};



}
}
}
#endif
