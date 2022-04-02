/*  Pokemon Sword/Shield Pokemon Sprites
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "Common/Qt/QtJsonTools.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/ImageMatch/ImageCropper.h"
#include "PokemonSwSh_PokemonSprites.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{




struct PokemonSpriteDatabase{
    QString m_sprite_path;
    QString m_sprite_silhouette;
    QImage m_sprites;
    QImage m_silhouettes;
    std::map<std::string, PokemonSprite> m_slug_to_data;

    static PokemonSpriteDatabase& instance(){
        static PokemonSpriteDatabase data;
        return data;
    }
    PokemonSpriteDatabase()
        : m_sprite_path(RESOURCE_PATH() + "PokemonSwSh/PokemonSprites.png")
        , m_sprite_silhouette(RESOURCE_PATH() + "PokemonSwSh/PokemonSilhouettes.png")
        , m_sprites(m_sprite_path)
        , m_silhouettes(m_sprite_silhouette)
    {
        QJsonObject json = read_json_file(
            RESOURCE_PATH() + "PokemonSwSh/PokemonSprites.json"
        ).object();

        int width = json.find("spriteWidth")->toInt();
        int height = json.find("spriteHeight")->toInt();
        if (width <= 0){
            throw FileException(nullptr, PA_CURRENT_FUNCTION, "Invalid width.", m_sprite_path.toStdString());
        }
        if (height <= 0){
            throw FileException(nullptr, PA_CURRENT_FUNCTION, "Invalid height.", m_sprite_path.toStdString());
        }

        QJsonObject locations = json.find("spriteLocations")->toObject();
        for (auto iter = locations.begin(); iter != locations.end(); ++iter){
            std::string slug = iter.key().toStdString();
            QJsonObject obj = iter.value().toObject();
            int y = obj.find("top")->toInt();
            int x = obj.find("left")->toInt();

            PokemonSprite sprite;
            sprite.m_sprite = m_sprites.copy(x, y, width, height);
            sprite.m_silhouette = m_silhouettes.copy(x, y, width, height);

            if (sprite.m_sprite.format() != QImage::Format_ARGB32){
                sprite.m_sprite = sprite.m_sprite.convertToFormat(QImage::Format_ARGB32);
            }
            if (sprite.m_silhouette.format() != QImage::Format_ARGB32){
                sprite.m_silhouette = sprite.m_sprite.convertToFormat(QImage::Format_ARGB32);
            }

            sprite.m_icon = QIcon(QPixmap::fromImage(ImageMatch::trim_image_alpha(sprite.m_sprite)));
//            sprite.m_sprite_zoomed = trim_image(sprite.m_sprite_full);
//            sprite.m_silhouette_zoomed = trim_image(sprite.m_silhouette_full);

//            cout << width << " x " << height << endl;
//            cout << slug << ": " << sprite.m_sprite.width() << " x " << sprite.m_sprite.height() << endl;

            m_slug_to_data.emplace(
                slug,
                std::move(sprite)
            );
        }
    }
};


const std::map<std::string, PokemonSprite>& all_pokemon_sprites(){
    return PokemonSpriteDatabase::instance().m_slug_to_data;
}
const PokemonSprite& get_pokemon_sprite(const std::string& slug){
    const std::map<std::string, PokemonSprite>& database = PokemonSpriteDatabase::instance().m_slug_to_data;
    auto iter = database.find(slug);
    if (iter == database.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Pokemon slug not found in database: " + slug);
    }
    return iter->second;
}
const PokemonSprite* get_pokemon_sprite_nothrow(const std::string& slug){
    const std::map<std::string, PokemonSprite>& database = PokemonSpriteDatabase::instance().m_slug_to_data;
    auto iter = database.find(slug);
    if (iter == database.end()){
        return nullptr;
    }
    return &iter->second;
}



}
}
}
