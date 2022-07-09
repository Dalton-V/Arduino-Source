/*  Switch Date
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <QJsonArray>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateEdit>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonTools.h"
#include "Common/Qt/QtJsonTools.h"
#include "Tools/Tools.h"
#include "SwitchDateOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


const QString SwitchDate::OPTION_TYPE = "SwitchDate";


int SwitchDate_init = register_option(
    SwitchDate::OPTION_TYPE,
        [](const QJsonObject& obj){
        return std::unique_ptr<ConfigItem>(
            new SwitchDate(obj)
        );
    }
);

SwitchDate::SwitchDate(const QJsonObject& obj)
    : SingleStatementOption(obj)
    , SwitchDateBaseOption(SingleStatementOption::m_label, QDate(2000, 1, 1))
{
    load_default(from_QJson(json_get_value_throw(obj, JSON_DEFAULT)));
    load_current(from_QJson(json_get_value_throw(obj, JSON_CURRENT)));
}
QString SwitchDate::check_validity() const{
    return SwitchDateBaseOption::check_validity();
}
void SwitchDate::restore_defaults(){
    SwitchDateBaseOption::restore_defaults();
}
QJsonObject SwitchDate::to_json() const{
    QJsonObject root = SingleStatementOption::to_json();
    root.insert(JSON_DEFAULT, to_QJson(write_default()));
    root.insert(JSON_CURRENT, to_QJson(write_current()));
    return root;
}
std::string SwitchDate::to_cpp() const{
    std::string str;
    str += m_declaration.toUtf8().data();
    str += " = {";
    str += std::to_string(m_current.year());
    str += ", ";
    str += std::to_string(m_current.month());
    str += ", ";
    str += std::to_string(m_current.day());
    str += "};\r\n";
    return str;
}
QWidget* SwitchDate::make_ui(QWidget& parent){
    return new SwitchDateUI(parent, *this);
}

SwitchDateUI::SwitchDateUI(QWidget& parent, SwitchDate& value)
    : SwitchDateBaseWidget(parent, value)
{}



}
}



