/*  Egg Feedback
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_EggFeedback_H
#define PokemonAutomation_PokemonBDSP_EggFeedback_H

#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "CommonFramework/Tools/ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


void hatch_egg(ProgramEnvironment& env, const BotBaseContext& context, ConsoleHandle& console);
void hatch_party(ProgramEnvironment& env, const BotBaseContext& context, ConsoleHandle& console, size_t eggs = 5);

void withdraw_1st_column_from_overworld(ProgramEnvironment& env, const BotBaseContext& context, ConsoleHandle& console);


void release(ProgramEnvironment& env, const BotBaseContext& context, ConsoleHandle& console);



}
}
}
#endif
