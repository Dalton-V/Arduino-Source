/*  Clone Items (Menu Overlap Method)
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Tools/StatsTracking.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/InferenceInfra/InferenceSession.h"
#include "CommonFramework/InferenceInfra/InferenceRoutines.h"
#include "CommonFramework/Inference/ImageMatchDetector.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/PokemonBDSP_Settings.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_StartBattleDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Programs/PokemonBDSP_GameEntry.h"
#include "PokemonBDSP/Programs/PokemonBDSP_GameNavigation.h"
#include "PokemonBDSP/Programs/PokemonBDSP_RunFromBattle.h"
#include "PokemonBDSP/Programs/Eggs/PokemonBDSP_EggRoutines.h"
#include "PokemonBDSP_MenuOverlap.h"
#include "PokemonBDSP_CloneItemsMenuOverlap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


CloneItemsMenuOverlap_Descriptor::CloneItemsMenuOverlap_Descriptor()
    : RunnableSwitchProgramDescriptor(
        "PokemonBDSP:CloneItemsMenuOverlap",
        STRING_POKEMON + " BDSP", "Clone Items (Menu Overlap)",
        "ComputerControl/blob/master/Wiki/Programs/PokemonBDSP/CloneItemsMenuOverlap.md",
        "Clone 5 items at a time using the menu overlap glitch. "
        "<font color=\"red\">(This requires game versions 1.1.0 - 1.1.1. The glitch it relies on was patched in v1.1.2.)</font>",
        FeedbackType::REQUIRED, false,
        PABotBaseLevel::PABOTBASE_12KB
    )
{}

CloneItemsMenuOverlap::CloneItemsMenuOverlap(const CloneItemsMenuOverlap_Descriptor& descriptor)
    : SingleSwitchProgramInstance(descriptor)
    , GO_HOME_WHEN_DONE(false)
    , BATCHES(
        "<b>Batches to Clone:</b>",
        999, 0, 999
    )
    , SAVE_INTERVAL(
        "<b>Save Game Interval:</b><br>"
        "The menu overlap glitch has a risk of softlocking the game and losing your progress. "
        "Automatically save the game every this many batches. Zero disables saving. <b>(This is a dangerous feature.)</b>",
        0, 0, 999
    )
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
    , m_advanced_options(
        "<font size=4><b>Advanced Options:</b> You should not need to touch anything below here.</font>"
    )
    , EXIT_BATTLE_TIMEOUT(
        "<b>Exit Battle Timeout:</b><br>After running, wait this long to return to overworld.",
        "10 * TICKS_PER_SECOND"
    )
{
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(BATCHES);
    PA_ADD_OPTION(SAVE_INTERVAL);
    PA_ADD_OPTION(NOTIFICATIONS);
    PA_ADD_OPTION(m_advanced_options);
    PA_ADD_OPTION(EXIT_BATTLE_TIMEOUT);
}


struct CloneItemsMenuOverlap::Stats : public StatsTracker{
    Stats()
        : m_batches(m_stats["Batches Cloned"])
        , m_errors(m_stats["Errors"])
        , m_resets(m_stats["Resets"])
    {
        m_display_order.emplace_back("Batches Cloned");
        m_display_order.emplace_back("Errors");
        m_display_order.emplace_back("Resets");
    }
    std::atomic<uint64_t>& m_batches;
    std::atomic<uint64_t>& m_errors;
    std::atomic<uint64_t>& m_resets;
};
std::unique_ptr<StatsTracker> CloneItemsMenuOverlap::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}


bool CloneItemsMenuOverlap::trigger_encounter(ProgramEnvironment& env, ConsoleHandle& console, BotBaseContext& context){
    console.log("Detected overworld. Triggering battle with menu overlap...");

    StartBattleMenuOverlapDetector detector(console);
    InferenceSession session(
        context, console,
        {{detector}}
    );

    for (size_t c = 0; c < 60; c++){
        if (detector.detected()){
            break;
        }
        pbf_move_left_joystick(context, 255, 128, 55, 0);
        pbf_move_right_joystick(context, 0, 128, 75, 0);
        context.wait_for_all_requests();

        if (detector.detected()){
            break;
        }
        pbf_move_left_joystick(context, 0, 128, 45, 0);
        pbf_move_right_joystick(context, 255, 128, 75, 0);
        context.wait_for_all_requests();
    }

    if (session.triggered_ptr()){
        console.log("Battle started!");
        return true;
    }else{
        console.log("No battle detected after 2 minutes.", COLOR_RED);
        return false;
    }
}
void CloneItemsMenuOverlap::swap_party(ConsoleHandle& console, BotBaseContext& context){
//    const uint16_t BOX_SCROLL_DELAY = GameSettings::instance().BOX_SCROLL_DELAY;
//    const uint16_t BOX_PICKUP_DROP_DELAY = GameSettings::instance().BOX_PICKUP_DROP_DELAY;
    const uint16_t BOX_SCROLL_DELAY = 20;
    const uint16_t BOX_PICKUP_DROP_DELAY = 40;

    //  Enter Box
    pbf_mash_button(context, BUTTON_ZL, 30);
    pbf_wait(context, 130);
    pbf_press_button(context, BUTTON_R, 20, 190);

    //  Change to multi-select.
    pbf_press_button(context, BUTTON_Y, 20, 50);
    pbf_press_button(context, BUTTON_Y, 20, 50);

    pbf_move_right_joystick(context, 0, 128, 10, BOX_SCROLL_DELAY);
    pbf_move_right_joystick(context, 128, 255, 10, BOX_SCROLL_DELAY);

    //  Deposit current column.
    pickup_column(context);
    party_to_column(context, 0);
    pbf_press_button(context, BUTTON_ZL, 10, BOX_PICKUP_DROP_DELAY);

    pbf_move_right_joystick(context, 255, 128, 10, BOX_SCROLL_DELAY);
    pickup_column(context);
    column_to_party(context, 1);
    pbf_press_button(context, BUTTON_ZL, 10, BOX_PICKUP_DROP_DELAY);
}
void CloneItemsMenuOverlap::mash_B_to_battle(ConsoleHandle& console, BotBaseContext& context){
    BattleMenuWatcher detector(BattleType::STANDARD);
    int ret = run_until(
        console, context,
        [=](BotBaseContext& context){
            pbf_mash_button(context, BUTTON_B, 10 * TICKS_PER_SECOND);
        },
        { &detector }
    );
    if (ret < 0){
        throw OperationFailedException(console, "Battle menu not detected after 10 seconds.");
    }else{
        console.log("Battle menu found!");
    }
    pbf_mash_button(context, BUTTON_B, 2 * TICKS_PER_SECOND);
}
void CloneItemsMenuOverlap::detach_items(ConsoleHandle& console, BotBaseContext& context){
    const uint16_t BOX_SCROLL_DELAY = GameSettings::instance().BOX_SCROLL_DELAY_0;

    for (size_t c = 0; c < 5; c++){
        if (c == 0){
            pbf_press_button(context, BUTTON_X, 10, 50);
        }else{
            pbf_move_right_joystick(context, 128, 255, 10, BOX_SCROLL_DELAY);
        }
        pbf_press_button(context, BUTTON_ZL, 10, 50);
        pbf_move_right_joystick(context, 128, 255, 10, 50);
        pbf_press_button(context, BUTTON_ZL, 10, 100);
        pbf_press_button(context, BUTTON_ZL, 10, 100);
        pbf_press_button(context, BUTTON_B, 10, 100);
    }
}

void CloneItemsMenuOverlap::program(SingleSwitchProgramEnvironment& env, BotBaseContext& context){
    Stats& stats = env.stats<Stats>();

    //  Connect the controller.
    pbf_mash_button(context, BUTTON_B, 50);

    size_t consecutive_failures = 0;
    uint16_t save_counter = 0;
    for (uint16_t batch = 0; batch < BATCHES; batch++){
        env.update_stats();
        send_program_status_notification(
            env.logger(), NOTIFICATION_STATUS_UPDATE,
            env.program_info(),
            "",
            stats.to_str()
        );

        QImage start = activate_menu_overlap_from_overworld(env.console, context);
        if (start.isNull()){
            stats.m_errors++;
            consecutive_failures++;
            if (consecutive_failures >= 3){
                throw OperationFailedException(env.console, "Failed to activate menu overlap glitch 3 times in the row.");
            }
            pbf_mash_button(context, BUTTON_B, 10 * TICKS_PER_SECOND);
            continue;
        }
        consecutive_failures = 0;

        //  Trigger an encounter.
        if (!trigger_encounter(env, env.console, context)){
            stats.m_errors++;
            pbf_mash_button(context, BUTTON_B, 10 * TICKS_PER_SECOND);
            continue;
        }

        //  Wait one second to avoid black screen.
        context.wait_for(std::chrono::seconds(1));

        swap_party(env.console, context);

        mash_B_to_battle(env.console, context);

        //  Run away.
        pbf_press_dpad(context, DPAD_UP, 10, 0);
        if (!run_from_battle(env.console, context, EXIT_BATTLE_TIMEOUT)){
            env.log("Detected likely black screen freeze. Resetting game...", COLOR_RED);
            stats.m_resets++;
            pbf_press_button(context, BUTTON_HOME, 10, GameSettings::instance().GAME_TO_HOME_DELAY);
            reset_game_from_home(env, env.console, context, ConsoleSettings::instance().TOLERATE_SYSTEM_UPDATE_MENU_FAST);
            continue;
        }
        pbf_mash_button(context, BUTTON_B, GameSettings::instance().MENU_TO_OVERWORLD_DELAY);


        //  Move column.
        start = env.console.video().snapshot();
        overworld_to_box(context);
        pbf_press_button(context, BUTTON_Y, 20, 40);
        pbf_press_button(context, BUTTON_Y, 20, 40);
        pickup_column(context);
        pbf_move_right_joystick(context, 255, 128, 10, GameSettings::instance().BOX_SCROLL_DELAY_0);
        pbf_press_button(context, BUTTON_ZL, 10, GameSettings::instance().BOX_PICKUP_DROP_DELAY);

        //  Detach items.
        detach_items(env.console, context);
        pbf_mash_button(context, BUTTON_B, 2 * TICKS_PER_SECOND);
        back_out_to_overworld_with_overlap(env.console, context, start, 0);

        save_counter++;
        if (SAVE_INTERVAL != 0 && save_counter >= SAVE_INTERVAL){
            save_game(context);
        }

        stats.m_batches++;
    }

    env.update_stats();
    send_program_finished_notification(
        env.logger(), NOTIFICATION_PROGRAM_FINISH,
        env.program_info(),
        "",
        stats.to_str()
    );
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}



}
}
}
