/*  Panel List
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <QFile>
#include <QTextStream>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Tools/PersistentSettings.h"
#include "UI/MainWindow.h"
#include "JsonSettings.h"
#include "JsonProgram.h"
#include "PanelList.h"

#include <iostream>
using std::cout;
using std::endl;

namespace PokemonAutomation{

PanelList::PanelList(QWidget& parent, MainWindow& window, const JsonValue& json)
    : QListWidget(&parent)
    , m_window(window)
{
    const JsonObject& category = json.get_object_throw();

    const std::string& category_name = category.get_string_throw("Name");
    m_display_name = category.get_string_throw("Display");

    //  Populate Settings
//    std::vector<QString> settings_list;
    bool first = true;
    for (const auto& item : category.get_array_throw("Settings")){
        const std::string& setting = item.get_string_throw();
//        settings.emplace_back(setting.toString());
//        m_list.emplace_back();
//        cout << setting.toUtf8().data() << endl;
        try{
            std::string path = settings.path + CONFIG_FOLDER_NAME + "/" + category_name + "/" + setting + ".json";
            std::unique_ptr<RightPanel> config(new Settings_JsonFile(category_name, path));
//            m_list.emplace_back(std::move(config));
            const std::string& name = config->name();
            if (!m_map.emplace(name, std::move(config)).second){
                throw ParseException("Duplicate: Program name");
            }
            if (first){
                addItem("---- Settings ----");
                QListWidgetItem* list_item = this->item(this->count() - 1);
                QFont font = list_item->font();
                font.setBold(true);
                list_item->setFont(font);
                first = false;
            }
            addItem(QString::fromStdString(name));
        }catch (const Exception& e){
            cout << "Error: " << e.message() << endl;
        }
    }

    //  Populate Programs
    std::string path = settings.path + SOURCE_FOLDER_NAME + "/" + category_name + "/ProgramList.txt";
    QFile file(QString::fromStdString(path));
    if (file.open(QFile::ReadOnly)){
        cout << "File = " << path << endl;
        QTextStream stream(&file);
        while (!stream.atEnd()){
            std::string line = stream.readLine().toStdString();
            if (line.empty()){
                continue;
            }
            cout << "Open: " << line << endl;

            //  Divider
            if (line[0] == '-'){
                addItem(QString::fromStdString(line));
                QListWidgetItem* list_item = this->item(this->count() - 1);
                QFont font = list_item->font();
                font.setBold(true);
                list_item->setFont(font);
                continue;
            }

            //  Program
            try{
                std::string path = settings.path + CONFIG_FOLDER_NAME + "/" + category_name + "/" + line + ".json";
                std::unique_ptr<RightPanel> config(new Program_JsonFile(category_name, path));
//                m_list.emplace_back(std::move(config));
                const std::string& name = config->name();
                if (!m_map.emplace(name, std::move(config)).second){
                    throw ParseException("Duplicate: Program name");
                }
                addItem(QString::fromStdString(name));
            }catch (const Exception& e){
                cout << "Error: " << e.message() << endl;
            }
        }
        file.close();
    }


    connect(this, &QListWidget::itemClicked, this, &PanelList::row_selected);
}

void PanelList::row_selected(QListWidgetItem* item){
    auto iter = m_map.find(item->text().toStdString());
    if (iter == m_map.end()){
//        std::cout << item->text().toUtf8().data() << std::endl;
//        PA_THROW_StringException("Invalid program name: " + item->text());
        return;
    }
    m_window.change_panel(*iter->second);
}


}
