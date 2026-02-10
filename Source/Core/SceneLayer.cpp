#include "Core/SceneLayer.h"
#include "Core/Xml.h"
#include <string>

namespace Elysium {

static SceneLayerSpace ParseSceneLayerSpace(const char* str) {
    if (!str) return SceneLayerSpace::World2D;
    std::string s = str;
    if (s == "Screen" || s == "Screen2D") return SceneLayerSpace::Screen2D;
    return SceneLayerSpace::World2D;
}

static SceneLayerBlend ParseSceneLayerBlend(const char* str) {
    if (!str) return SceneLayerBlend::Normal;
    std::string s = str;
    if (s == "Additive") return SceneLayerBlend::Additive;
    if (s == "Multiply") return SceneLayerBlend::Multiply;
    if (s == "Alpha") return SceneLayerBlend::Alpha;
    return SceneLayerBlend::Normal;
}

void SceneLayer::LoadXml(SceneLayer& layer, tinyxml2::XMLElement* el) {
    const char* name = el->Attribute("name");
    layer.name = name ? name : "default";
    layer.zIndex = el->IntAttribute("z", 0);
    layer.opacity = el->FloatAttribute("opacity", 1.0f);
    layer.isVisible = el->BoolAttribute("isVisible", true);
    layer.isComposited = el->BoolAttribute("isComposited", false);
    layer.space = ParseSceneLayerSpace(el->Attribute("space"));

    // Support both "blend" (shorthand) and "layerBlend" attributes
    const char* layerBlendAttr = el->Attribute("layerBlend");
    layer.layerBlend = ParseSceneLayerBlend(layerBlendAttr);

    // compositeBlend defaults to layerBlend if not specified
    const char* compositeBlendAttr = el->Attribute("compositeBlend");
    layer.compositeBlend = ParseSceneLayerBlend(compositeBlendAttr);

    // Parse ambient color
    const char* ambientStr = el->Attribute("ambient");
    if (ambientStr) {
        layer.ambient = ParseHexColor(ambientStr, {0, 0, 0, 0});
    }
}

} // namespace Elysium
