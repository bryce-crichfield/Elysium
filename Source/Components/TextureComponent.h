#pragma once
#include "Core/Component.h"
#include "raylib.h"

namespace Elysium {
    enum class TextureFilterMode { Point, Bilinear };

    // The resolved renderable result of an animation/asset lookup (e.g. SpriteSystem
    // resolving SpriteComponent's spriteName/sheet/sequence/frame). RenderSystem draws
    // straight from this instead of re-walking the sprite/sheet/sequence maps itself.
    // textureName/sourceRect are owned by whatever system resolves them (re-written on
    // every frame-change); tint/origin default from the source asset on first resolve
    // but are otherwise left alone so scripts/the inspector can override them per-instance.
    struct TextureComponent {
        std::string textureName;
        Rectangle sourceRect = {0, 0, 0, 0};
        Color tint = WHITE;
        float originX = 0.5f;
        float originY = 0.5f;
        TextureFilterMode filterMode = TextureFilterMode::Point;

        static constexpr const char* Name() { return "Texture"; }
        static constexpr const char* XmlTag() { return "TextureComponent"; }

        static void LoadXml(TextureComponent& c, tinyxml2::XMLElement* el);
        static void SaveXml(const TextureComponent& c, XMLBuilder& builder);
        static void Inspect(TextureComponent& c, Entity e);
        static void BindLua(sol::usertype<TextureComponent>& ut);
    };
}
