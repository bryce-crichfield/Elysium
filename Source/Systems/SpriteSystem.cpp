#include "Systems/SpriteSystem.h"
#include "Core/Path.h"
#include "Core/SystemRegistry.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Components/MaterialComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/SpriteComponent.h"
#include "Core/Graphics.h"
#include "Core/Sprite.h"

#include "Interfaces/IAssetService.h"
#include "Services/AssetService.h"

namespace Elysium::Systems {

SpriteSystem::SpriteSystem(Context context) : System(context) {
}

// The entity's Texture layer, created (first in the stack, so any other layers draw
// over the sprite) along with its MaterialComponent if missing.
static MaterialLayer& EnsureTextureLayer(Elysium::World* world, Entity entity) {
    if (!world->HasComponent<MaterialComponent>(entity)) world->AddComponent<MaterialComponent>(entity, MaterialComponent{});
    auto& material = world->GetComponent<MaterialComponent>(entity);
    for (MaterialLayer& layer : material.layers) {
        if (layer.material == "Texture") return layer;
    }
    MaterialLayer layer;
    layer.material = "Texture";
    material.layers.insert(material.layers.begin(), std::move(layer));
    return material.layers.front();
}

// Resolves spriteName/sheetName/sequenceName/sequenceIndex to a sheet texture + frame
// rect and writes it into the entity's shape: a RectangleComponent sized to
// the frame (pivoting at the sprite's origin) carrying a Texture material layer whose
// uSourceRect selects the frame. Both are added on first
// resolve. Tint and any other layer overrides are left alone so they stay per-instance.
static void ResolveSprite(Elysium::World* world, Elysium::Services::IAssetService& assets,
                          Entity entity, const SpriteComponent& spriteComp) {
    auto* spriteData = assets.Get<Sprite>(Path(spriteComp.spriteName));
    if (!spriteData) return;
    const Sprite& sprite = *spriteData;
    if (sprite.name.empty()) return;

    auto sheetIt = sprite.sheets.find(spriteComp.sheetName);
    if (sheetIt == sprite.sheets.end()) return;
    const SpriteSheet& sheet = sheetIt->second;

    auto seqIt = sheet.sequences.find(spriteComp.sequenceName);
    if (seqIt == sheet.sequences.end()) return;
    const SpriteSequence& sequence = seqIt->second;
    if (sequence.indices.empty()) return;

    size_t frameIdx = spriteComp.sequenceIndex % sequence.indices.size();
    size_t linearIndex = sequence.indices[frameIdx];

    std::string texturePath = "Sprites/" + sheet.path;
    auto* textureData = assets.Get<Texture>(Path(texturePath));
    if (!textureData) return;
    const Texture& texture = *textureData;
    if (texture.id == 0) return;

    float frameWidth  = (float)texture.width  / (float)sheet.cols;
    float frameHeight = (float)texture.height / (float)sheet.rows;
    size_t col = linearIndex % sheet.cols;
    size_t row = linearIndex / sheet.cols;

    if (!world->HasComponent<RectangleComponent>(entity)) {
        world->AddComponent<RectangleComponent>(entity, RectangleComponent{1, 1});
    }
    // Frame-sized; the renderer applies the entity's scale (negative mirrors) and rotation.
    auto& rect = world->GetComponent<RectangleComponent>(entity);
    rect.width = frameWidth;
    rect.height = frameHeight;
    rect.originX = sprite.originX;
    rect.originY = sprite.originY;

    MaterialLayer& layer = EnsureTextureLayer(world, entity);
    layer.texturePath = texturePath;
    layer.overrides["uSourceRect"] = Value{Vector4{col * frameWidth, row * frameHeight, frameWidth, frameHeight}};
}

void SpriteSystem::Update(float deltaTime) {
    auto& assets = services->Get<Services::IAssetService>();

    world->Query<SpriteComponent>([&](Entity entity, auto& spriteComp) {
        spriteComp.frameElapsed += deltaTime;
        if (spriteComp.frameElapsed >= spriteComp.frameDuration) {
            spriteComp.frameElapsed -= spriteComp.frameDuration;

            auto* spriteData = assets.Get<Sprite>(Path(spriteComp.spriteName));
            if (!spriteData || spriteData->name.empty()) return;
            const Sprite& sprite = *spriteData;

            auto sheetIt = sprite.sheets.find(spriteComp.sheetName);
            if (sheetIt == sprite.sheets.end()) return;
            const SpriteSequence* sequence = nullptr;
            auto seqIt = sheetIt->second.sequences.find(spriteComp.sequenceName);
            if (seqIt != sheetIt->second.sequences.end()) sequence = &seqIt->second;

            if (sequence && !sequence->indices.empty()) {
                // Advance to next frame (loop back to 0 when reaching end)
                spriteComp.sequenceIndex = (spriteComp.sequenceIndex + 1) % sequence->indices.size();
            }
        }

        // Every frame, not just on frame advance: a script can swap sheet/sequence or
        // flip the scale at any time, and the lookups are a few hash probes.
        ResolveSprite(world, assets, entity, spriteComp);
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::SpriteSystem)
