/*  Single Switch Program Template
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch_SingleSwitchProgramWidget.h"

namespace PokemonAutomation{
namespace NintendoSwitch{



SingleSwitchProgramInstance::SingleSwitchProgramInstance(const RunnableSwitchProgramDescriptor& descriptor)
    : RunnableSwitchProgramInstance(descriptor)
    , m_switch(
        0,
        descriptor.min_pabotbase_level(),
        descriptor.feedback(),
        descriptor.allow_commands_while_running()
    )
{
    m_setup = &m_switch;
}
QWidget* SingleSwitchProgramInstance::make_widget(QWidget& parent, PanelHolder& holder){
    return SingleSwitchProgramWidget::make(parent, *this, holder);
}







}
}
