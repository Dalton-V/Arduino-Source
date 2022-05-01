/*  Catchability Selector
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <QJsonValue>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include "Common/Qt/NoWheelComboBox.h"
#include "CommonFramework/Globals.h"
#include "PokemonSwSh_Catchability.h"


namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{




class CatchabilitySelectorWidget : public QWidget, public ConfigWidget{
public:
    CatchabilitySelectorWidget(QWidget& parent, CatchabilitySelectorOption& value);

    virtual void restore_defaults() override;
    virtual void update_ui() override;

private:
    CatchabilitySelectorOption& m_value;
    QComboBox* m_box;
};




CatchabilitySelectorOption::CatchabilitySelectorOption()
    : m_label("<b>" + STRING_POKEMON + " Catchability</b>")
    , m_default(Catchability::ALWAYS_CATCHABLE)
    , m_current(Catchability::ALWAYS_CATCHABLE)
{}
void CatchabilitySelectorOption::load_json(const QJsonValue& json){
    size_t index = json.toInt((int)m_default);
    index = std::min(index, (size_t)2);
    m_current = (Catchability)index;
}
QJsonValue CatchabilitySelectorOption::to_json() const{
    return QJsonValue((int)m_current);
}

void CatchabilitySelectorOption::restore_defaults(){
    m_current = m_default;
};

ConfigWidget* CatchabilitySelectorOption::make_ui(QWidget& parent){
    return new CatchabilitySelectorWidget(parent, *this);
}




CatchabilitySelectorWidget::CatchabilitySelectorWidget(QWidget& parent, CatchabilitySelectorOption& value)
    : QWidget(&parent)
    , ConfigWidget(value, *this)
    , m_value(value)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    QLabel* text = new QLabel(m_value.m_label, this);
    layout->addWidget(text, 1);
    text->setWordWrap(true);
    m_box = new NoWheelComboBox(&parent);
    layout->addWidget(m_box);
    m_box->addItem("Always Catchable");
    m_box->addItem("Maybe Uncatchable");
    m_box->addItem("Never Catchable");
    m_box->setCurrentIndex((int)m_value.m_current);
    layout->addWidget(m_box, 1);

    connect(
        m_box, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [=](int index){
            m_value.m_current = (Catchability)index;
        }
    );
}
void CatchabilitySelectorWidget::restore_defaults(){
    m_value.restore_defaults();
    update_ui();
}
void CatchabilitySelectorWidget::update_ui(){
    m_box->setCurrentIndex((int)(Catchability)m_value);
}



}
}
}
