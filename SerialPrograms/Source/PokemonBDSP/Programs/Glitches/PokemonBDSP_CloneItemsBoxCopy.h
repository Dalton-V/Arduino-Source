/*  Clone Items (Box Swap Method)
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_CloneItemsBoxSwap_H
#define PokemonAutomation_PokemonBDSP_CloneItemsBoxSwap_H

#include "CommonFramework/Options/StaticTextOption.h"
#include "CommonFramework/Options/SimpleIntegerOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/Options/GoHomeWhenDoneOption.h"
//#include "NintendoSwitch/Options/TimeExpressionOption.h"
#include "NintendoSwitch/Framework/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


class CloneItemsBoxCopy_Descriptor : public RunnableSwitchProgramDescriptor{
public:
    CloneItemsBoxCopy_Descriptor();
};



class CloneItemsBoxCopy : public SingleSwitchProgramInstance{
public:
    CloneItemsBoxCopy(const CloneItemsBoxCopy_Descriptor& descriptor);

    virtual std::unique_ptr<StatsTracker> make_stats() const override;
    virtual void program(SingleSwitchProgramEnvironment& env, const BotBaseContext& context) override;

private:
    struct Stats;

private:
    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;
    SimpleIntegerOption<uint16_t> BOXES;

    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};




}
}
}
#endif
