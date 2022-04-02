/*  Ball Thrower
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Device.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonSwSh/PokemonSwSh_Settings.h"
#include "PokemonSwSh_BallThrower.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{


BallThrower_Descriptor::BallThrower_Descriptor()
    : RunnableSwitchProgramDescriptor(
        "PokemonSwSh:BallThrower",
        STRING_POKEMON + " SwSh", "Ball Thrower",
        "ComputerControl/blob/master/Wiki/Programs/PokemonSwSh/BallThrower.md",
        "Blindly throw balls at the opposing " + STRING_POKEMON + " until it catches.",
        FeedbackType::NONE, false,
        PABotBaseLevel::PABOTBASE_12KB
    )
{}



BallThrower::BallThrower(const BallThrower_Descriptor& descriptor)
    : SingleSwitchProgramInstance(descriptor)
{
    PA_ADD_OPTION(START_IN_GRIP_MENU);
}

void BallThrower::program(SingleSwitchProgramEnvironment& env, const BotBaseContext& context){
    if (START_IN_GRIP_MENU){
        grip_menu_connect_go_home(env.console);
        pbf_press_button(env.console, BUTTON_HOME, 10, GameSettings::instance().HOME_TO_GAME_DELAY);
    }else{
        pbf_press_button(env.console, BUTTON_X, 5, 5);
    }

    while (true){
        pbf_press_button(env.console, BUTTON_X, 50, 50);
        pbf_press_button(env.console, BUTTON_A, 50, 50);
        pbf_mash_button(env.console, BUTTON_B, 100);
    }

    pbf_press_button(env.console, BUTTON_HOME, 10, GameSettings::instance().GAME_TO_HOME_DELAY_SAFE);
}



}
}
}
