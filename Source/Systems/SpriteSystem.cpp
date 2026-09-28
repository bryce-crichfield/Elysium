#include <algorithm>
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

// The sheet and sequence a component shows. Unset or unknown names fall back to the
// sprite's first sheet (by name) and its "default" sequence (else its first), so a sprite
// shows something the moment it's picked, before a script or the inspector names them.
struct ResolvedFrames {
    const SpriteSheet* sheet = nullptr;
    const SpriteSequence* sequence = nullptr;
};

static ResolvedFrames ResolveFrames(const Sprite& sprite, const SpriteComponent& spriteComp) {
    ResolvedFrames out;
    auto sheetIt = sprite.sheets.find(spriteComp.sheetName);
    if (sheetIt == sprite.sheets.end()) {
        sheetIt = std::min_element(sprite.sheets.begin(), sprite.sheets.end(),
                                   [](const auto& a, const auto& b) { return a.first < b.first; });
    }
    if (sheetIt == sprite.sheets.end()) return out;
    out.sheet = &sheetIt->second;

    const auto& sequences = out.sheet->sequences;
    auto seqIt = sequences.find(spriteComp.sequenceName);
    if (seqIt == sequences.end()) seqIt = sequences.find("default");
    if (seqIt == sequences.end()) {
        seqIt = std::min_element(sequences.begin(), sequences.end(),
                                 [](const auto& a, const auto& b) { return a.first < b.first; });
    }
    if (seqIt != sequences.end() && !seqIt->second.indices.empty()) out.sequence = &seqIt->second;
    return out;
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

    const ResolvedFrames frames = ResolveFrames(sprite, spriteComp);
    if (!frames.sheet || !frames.sequence) return;
    const SpriteSheet& sheet = *frames.sheet;
    const SpriteSequence& sequence = *frames.sequence;

    size_t frameIdx = spriteComp.sequenceIndex % sequence.indices.size();
    size_t linearIndex = sequence.indices[frameIdx];

    auto* textureData = assets.Get<Texture>(Path(sheet.path));
    if (!textureData) return;
    const Texture& texture = *textureData;
    if (texture.id == 0) return;

    float frameWidth  = (float)texture.width  / (float)sheet.cols;
    float frameHeight = (float)texture.height / (float)sheet.rows;
    size_t col = linearIndex % sheet.cols;
    size_t row = linearIndex / sheet.cols;

    layer->texturePath = sheet.path;
    layer->normalMapPath = sheet.normalPath;
    layer->emissionMapPath = sheet.emissionPath;
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

    // Runs while paused, and animates there too, so the editor (and prefab documents, which
    // never play) previews every sprite playing.
    world->Query<SpriteComponent>([&](Entity entity, auto& spriteComp) {
        spriteComp.frameElapsed += deltaTime;
        if (spriteComp.frameElapsed >= spriteComp.frameDuration) {
            spriteComp.frameElapsed -= spriteComp.frameDuration;

            auto* spriteData = assets.Get<Sprite>(Path(spriteComp.spriteName));
            if (!spriteData || spriteData->name.empty()) return;
            const Sprite& sprite = *spriteData;

            const SpriteSequence* sequence = ResolveFrames(sprite, spriteComp).sequence;

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
