/*  Pokemon Ball Select Widget
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "Pokemon_IVCheckerWidget.h"

namespace PokemonAutomation{
namespace Pokemon{


IVCheckerFilterWidget::IVCheckerFilterWidget(QWidget& parent, IVCheckerFilter current)
    : NoWheelComboBox(&parent)
{
    const std::string& current_str = IVCheckerFilter_enum_to_string(current);
    for (size_t index = 0; index < IVCheckerFilter_NAMES.size(); index++){
        const std::string& str = IVCheckerFilter_NAMES[index];
        this->addItem(QString::fromStdString(str));
        if (str == current_str){
            this->setCurrentIndex((int)index);
        }
    }
}

IVCheckerFilterWidget::operator IVCheckerFilter() const{
    int index = this->currentIndex();
    if (index < 0 || index > (int)IVCheckerValue::Best){
        return IVCheckerFilter::Anything;
    }
    return (IVCheckerFilter)index;
}



}
}
