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
#include "Interfaces/ISceneService.h"
#include "Services/AssetService.h"

namespace Elysium::Systems {

SpriteSystem::SpriteSystem(Context context) : System(context) {
}

// The entity's Texture material layer, or null. Never created: an entity opts into
// drawing its sprite by authoring a MaterialComponent with a Texture layer.
static MaterialLayer* FindTextureLayer(Elysium::World* world, Entity entity) {
    if (!world->HasComponent<MaterialComponent>(entity)) return nullptr;
    for (MaterialLayer& layer : world->GetComponent<MaterialComponent>(entity).layers) {
        if (layer.material == "Texture") return &layer;
    }
    return nullptr;
}

// Resolves spriteName/sheetName/sequenceName/sequenceIndex to a sheet texture + frame rect
// and writes it into the entity's Texture material layer (texturePath + uSourceRect clip).
// A RectangleComponent, if present, is sized to the frame and pivots at the sprite's
// origin. Nothing is added: without a MaterialComponent + Texture layer this is a no-op.
static void ResolveSprite(Elysium::World* world, Elysium::Services::IAssetService& assets,
                          Entity entity, const SpriteComponent& spriteComp) {
    MaterialLayer* layer = FindTextureLayer(world, entity);
    if (!layer) return;

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

    layer->texturePath = texturePath;
    layer->overrides["uSourceRect"] = Value{Vector4{col * frameWidth, row * frameHeight, frameWidth, frameHeight}};

    // Frame-sized; the renderer applies the entity's scale (negative mirrors) and rotation.
    if (world->HasComponent<RectangleComponent>(entity)) {
        auto& rect = world->GetComponent<RectangleComponent>(entity);
        rect.width = frameWidth;
        rect.height = frameHeight;
        rect.originX = sprite.originX;
        rect.originY = sprite.originY;
    }
}

void SpriteSystem::Update(float deltaTime) {
    auto& assets = services->Get<Services::IAssetService>();

    // Runs while paused so the editor (and prefab documents, which never play) still shows
    // each sprite's current frame; animation only advances during simulation.
    auto& sceneService = services->Get<Services::ISceneService>();
    const bool animate = sceneService.IsPlaying() && sceneService.GetEditorScene() != scene;

    world->Query<SpriteComponent>([&](Entity entity, auto& spriteComp) {
        if (animate) spriteComp.frameElapsed += deltaTime;
        if (animate && spriteComp.frameElapsed >= spriteComp.frameDuration) {
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
