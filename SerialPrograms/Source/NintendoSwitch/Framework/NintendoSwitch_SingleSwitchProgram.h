/*  Single Switch Program
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_SingleSwitchProgram_H
#define PokemonAutomation_NintendoSwitch_SingleSwitchProgram_H

#include "CommonFramework/Globals.h"
#include "CommonFramework/ControllerDevices/SerialPortGlobals.h"
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "CommonFramework/Tools/ConsoleHandle.h"
#include "CommonFramework/Panels/RunnablePanel.h"

namespace PokemonAutomation{
    class BotBaseContext;
namespace NintendoSwitch{


class SingleSwitchProgramInstance2;


class SingleSwitchProgramEnvironment : public ProgramEnvironment{
public:
    ConsoleHandle console;

private:
    friend class SingleSwitchProgramSession;
    friend class SingleSwitchProgramWidget;
    template <class... Args>
    SingleSwitchProgramEnvironment(
        const ProgramInfo& program_info,
        CancellableScope& scope,
        Logger& logger,
        StatsTracker* current_stats,
        const StatsTracker* historical_stats,
        Args&&... args
    )
        : ProgramEnvironment(program_info, logger, current_stats, historical_stats)
        , console(0, std::forward<Args>(args)...)
    {
        console.initialize_inference_threads(scope, inference_dispatcher());
    }
};


class SingleSwitchProgramDescriptor : public RunnablePanelDescriptor{
public:
    SingleSwitchProgramDescriptor(
        std::string identifier,
        std::string category, std::string display_name,
        std::string doc_link,
        std::string description,
        FeedbackType feedback, bool allow_commands_while_running,
        PABotBaseLevel min_pabotbase_level
    );

    FeedbackType feedback() const{ return m_feedback; }
    PABotBaseLevel min_pabotbase_level() const{ return m_min_pabotbase_level; }
    bool allow_commands_while_running() const{ return m_allow_commands_while_running; }

    virtual std::unique_ptr<PanelInstance> make_panel() const override;
    virtual std::unique_ptr<SingleSwitchProgramInstance2> make_instance() const = 0;

private:
    const FeedbackType m_feedback;
    const PABotBaseLevel m_min_pabotbase_level;
    const bool m_allow_commands_while_running;
};






//
//  As of this writing, this class will never be called in a manner where
//  thread-safety is of concern. However, this may change in the future.
//
//  Here is the curent status:
//
//  Called from UI thread:
//    - Construction/destruction
//    - from/to_json()
//    - restore_defaults()
//
//  Called from program thread:
//    - program()
//
//  Called from both UI and program threads:
//    - check_validity()
//
//  Calls to this class will never be concurrent from different threads.
//
class SingleSwitchProgramInstance2{
public:
    virtual ~SingleSwitchProgramInstance2() = default;
    SingleSwitchProgramInstance2(const SingleSwitchProgramInstance2&) = delete;
    void operator=(const SingleSwitchProgramInstance2&) = delete;

    SingleSwitchProgramInstance2();

    virtual void program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) = 0;


public:
    //  Settings

    virtual void from_json(const JsonValue& json);
    virtual JsonValue to_json() const;

    virtual std::string check_validity() const;
    virtual void restore_defaults();


protected:
    friend class SingleSwitchProgramOption;

    BatchOption m_options;
    void add_option(ConfigOption& option, std::string serialization_string);


public:
    EventNotificationOption NOTIFICATION_PROGRAM_FINISH;
    EventNotificationOption NOTIFICATION_ERROR_RECOVERABLE;
    EventNotificationOption NOTIFICATION_ERROR_FATAL;
};




template <typename Descriptor, typename Instance>
class SingleSwitchProgramWrapper : public Descriptor{
public:
    virtual std::unique_ptr<SingleSwitchProgramInstance2> make_instance() const override{
        return std::unique_ptr<SingleSwitchProgramInstance2>(new Instance());
    }
};

template <typename Descriptor, typename Instance>
std::unique_ptr<PanelDescriptor> make_single_switch_program(){
    return std::make_unique<SingleSwitchProgramWrapper<Descriptor, Instance>>();
}








}
}
#endif

