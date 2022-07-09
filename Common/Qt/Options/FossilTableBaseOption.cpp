/*  Fossil Table Option
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <map>
#include <QVBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QLineEdit>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Qt/NoWheelComboBox.h"
#include "FossilTableBaseOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{


const std::string FossilGame::JSON_GAME_SLOT    = "game_slot";
const std::string FossilGame::JSON_USER_SLOT    = "user_slot";
const std::string FossilGame::JSON_FOSSIL       = "fossil";
const std::string FossilGame::JSON_REVIVES      = "revives";

const std::vector<QString> FossilGame::FOSSIL_LIST{
    "Dracozolt",
    "Arctozolt",
    "Dracovish",
    "Arctovish",
};
const std::map<QString, int> FOSSIL_MAP{
    {"Dracozolt", 0},
    {"Arctozolt", 1},
    {"Dracovish", 2},
    {"Arctovish", 3},
};





void FossilGame::load_json(const JsonValue& json){
    const JsonObject* obj = json.get_object();
    if (obj == nullptr){
        return;
    }

    obj->read_integer(game_slot, JSON_GAME_SLOT, 1, 2);
    obj->read_integer(user_slot, JSON_USER_SLOT, 1, 8);

    const std::string* str = obj->get_string(JSON_FOSSIL);
    if (str != nullptr){
        auto iter = FOSSIL_MAP.find(QString::fromStdString(*str));
        if (iter != FOSSIL_MAP.end()){
            fossil = (Fossil)iter->second;
        }
    }

    obj->read_integer(revives, JSON_REVIVES, 0, 965);
}
JsonValue FossilGame::to_json() const{
    JsonObject obj;
    obj[JSON_GAME_SLOT] = game_slot;
    obj[JSON_USER_SLOT] = user_slot;
    obj[JSON_FOSSIL] = FOSSIL_LIST[fossil].toStdString();
    obj[JSON_REVIVES] = revives;
    return obj;
}
std::unique_ptr<EditableTableRow> FossilGame::clone() const{
    return std::unique_ptr<EditableTableRow>(new FossilGame(*this));
}
std::vector<QWidget*> FossilGame::make_widgets(QWidget& parent){
    std::vector<QWidget*> widgets;
    widgets.emplace_back(make_game_slot_box(parent));
    widgets.emplace_back(make_user_slot_box(parent));
    widgets.emplace_back(make_fossil_slot_box(parent));
    widgets.emplace_back(make_revives_slot_box(parent));
    return widgets;
}
QWidget* FossilGame::make_game_slot_box(QWidget& parent){
    QComboBox* box = new NoWheelComboBox(&parent);
    box->addItem("Game 1");
    box->addItem("Game 2");
    box->setCurrentIndex((int)game_slot - 1);
    box->connect(
        box, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        box, [&](int index){
            game_slot = (uint8_t)(index + 1);
        }
    );
    return box;
}
QWidget* FossilGame::make_user_slot_box(QWidget& parent){
    QComboBox* box = new NoWheelComboBox(&parent);
    box->addItem("User 1");
    box->addItem("User 2");
    box->addItem("User 3");
    box->addItem("User 4");
    box->addItem("User 5");
    box->addItem("User 6");
    box->addItem("User 7");
    box->addItem("User 8");
    box->setCurrentIndex((int)user_slot - 1);
    box->connect(
        box, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        box, [&](int index){
            user_slot = (uint8_t)(index + 1);
        }
    );
    return box;
}
QWidget* FossilGame::make_fossil_slot_box(QWidget& parent){
    QComboBox* box = new NoWheelComboBox(&parent);
    box->addItem(FOSSIL_LIST[0]);
    box->addItem(FOSSIL_LIST[1]);
    box->addItem(FOSSIL_LIST[2]);
    box->addItem(FOSSIL_LIST[3]);
    box->setCurrentIndex(fossil);
    box->connect(
        box, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        box, [&](int index){
            fossil = (Fossil)index;
        }
    );
    return box;
}
QWidget* FossilGame::make_revives_slot_box(QWidget& parent){
    QLineEdit* box = new QLineEdit(&parent);
    QIntValidator* validator = new QIntValidator(0, 965, box);
    box->setValidator(validator);
    box->setText(QString::number(revives));
    box->setMaxLength(3);
    box->setMaximumWidth(50);
    box->setAlignment(Qt::AlignHCenter);
    box->connect(
        box, &QLineEdit::textChanged,
        box, [&, box](const QString& text){
            int raw = text.toInt();
            int fixed = raw;
            fixed = std::max(fixed, 0);
            fixed = std::min(fixed, 965);
            if (raw != fixed){
                box->setText(QString::number(fixed));
            }
            revives = fixed;
        }
    );
    return box;
}


QStringList FossilGameOptionFactory::make_header() const{
    QStringList list;
    list << "Game" << "User" << "Fossil" << "Revives";
    return list;
}
std::unique_ptr<EditableTableRow> FossilGameOptionFactory::make_row() const{
    return std::unique_ptr<EditableTableRow>(new FossilGame());
}







}
}
}









