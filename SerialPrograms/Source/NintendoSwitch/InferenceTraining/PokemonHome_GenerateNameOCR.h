/*  Pokemon Home Generate Name OCR
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonHome_GenerateNameOCR_H
#define PokemonAutomation_PokemonHome_GenerateNameOCR_H

#include "CommonFramework/OCR/OCR_LanguageOptionOCR.h"
#include "NintendoSwitch/Options/TimeExpressionOption.h"
#include "NintendoSwitch/Framework/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


class GenerateNameOCRData_Descriptor : public RunnableSwitchProgramDescriptor{
public:
    GenerateNameOCRData_Descriptor();
};



class GenerateNameOCRData : public SingleSwitchProgramInstance{

public:
    GenerateNameOCRData(const GenerateNameOCRData_Descriptor& descriptor);

    virtual void program(SingleSwitchProgramEnvironment& env, const BotBaseContext& context) override;

private:
    OCR::LanguageOCR LANGUAGE;
    TimeExpressionOption<uint16_t> DELAY;
};



}
}
}
#endif
