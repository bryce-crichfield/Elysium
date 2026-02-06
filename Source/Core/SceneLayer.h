#pragma once

#include <string>
#include <tinyxml2.h>
#include "raylib.h"

namespace Elysium {

enum class SceneLayerSpace {
    World2D,
    Screen2D,
};

enum class SceneLayerBlend {
    Normal,
    Additive,
    Multiply,
    Alpha,
};

struct SceneLayer {
    std::string name;
    int zIndex = 0;
    bool isVisible = true;
    bool isComposited = true;       // If composited, the layer will render to a render target instead of immediately to the framebuffer
    float opacity = 1.0f;

    SceneLayerSpace space = SceneLayerSpace::World2D;
    SceneLayerBlend layerBlend = SceneLayerBlend::Normal;      // how objects in this layer are blended when rendered with respect to each other
    SceneLayerBlend compositeBlend = SceneLayerBlend::Normal;  // how this layer is blended when composited onto the framebuffer

    Color ambient = {0, 0, 0, 0};

    static constexpr const char* XmlTag() { return "SceneLayer"; }

    static void LoadXml(SceneLayer& layer, tinyxml2::XMLElement* el);
};

} // namespace Elysium
