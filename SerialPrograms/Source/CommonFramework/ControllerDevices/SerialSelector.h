/*  Serial Connection Selector
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_SerialSelector_H
#define PokemonAutomation_SerialSelector_H

#include <memory>
#include <QString>
#include "Common/Cpp/Pimpl.h"
#include "Common/PokemonSwSh/PokemonProgramIDs.h"
#include "CommonFramework/Logging/LoggerQt.h"

class QSerialPortInfo;
class QWidget;

namespace PokemonAutomation{

class JsonValue;
class SerialSelectorWidget;


class SerialSelector{
public:
    ~SerialSelector();
    SerialSelector(
        QString label,
        PABotBaseLevel minimum_pabotbase
    );
    SerialSelector(
        QString label,
        PABotBaseLevel minimum_pabotbase,
        const JsonValue& json
    );

    void load_json(const JsonValue& json);
    JsonValue to_json() const;

    const QSerialPortInfo* port() const;

    SerialSelectorWidget* make_ui(QWidget& parent, LoggerQt& logger);

private:
    friend class SerialSelectorWidget;

    const QString m_label;
    const PABotBaseLevel m_minimum_pabotbase;

    Pimpl<QSerialPortInfo> m_port;
};





}
#endif


