/*  Pokemon Ball Select
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_Pokemon_PokemonBallSelect_H
#define PokemonAutomation_Pokemon_PokemonBallSelect_H

#include "CommonFramework/Options/StringSelectOption.h"

namespace PokemonAutomation{
namespace Pokemon{


struct PokemonBallSelectData{
    PokemonBallSelectData(const std::vector<std::string>& slugs);
    const std::vector<std::pair<std::string, QIcon>>& cases() const{ return m_list; }

protected:
    std::vector<std::pair<std::string, QIcon>> m_list;
};


class PokemonBallSelect : private PokemonBallSelectData, public StringSelectOption{
public:
    PokemonBallSelect(QString label, const std::string& default_slug = "");

    const std::string& slug() const;

private:
};


}
}
#endif
