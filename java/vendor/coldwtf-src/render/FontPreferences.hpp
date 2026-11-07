#pragma once
#include "core/Config.hpp"
#include <algorithm>
namespace krux::fonts {
enum class Domain { ClickGui, Hud, WorldLabels };
inline const char* preferenceKey(Domain domain) {
    switch(domain){case Domain::ClickGui:return "clickGuiFontStyle";case Domain::Hud:return "hudFontStyle";case Domain::WorldLabels:return "worldLabelFontStyle";}
    return "clickGuiFontStyle";
}
inline int preference(const cold::config::Document& config,Domain domain) {
    return std::clamp(config.getInt("interface",preferenceKey(domain),config.getInt("interface","fontStyle",1)),0,2);
}
}