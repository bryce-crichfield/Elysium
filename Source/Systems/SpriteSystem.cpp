#include "Systems/SpriteSystem.h"
#include "Core/Path.h"
#include "Core/SystemRegistry.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Components/SpriteComponent.h"
#include "Components/TextureComponent.h"

#include "Core/Application.h"
#include "Services/AssetService.h"

namespace Elysium::Systems {

SpriteSystem::SpriteSystem(Context context) : System(context) {
}

// Resolves spriteName/sheetName/sequenceName/sequenceIndex to a concrete texture +
// source rect and writes it into the entity's TextureComponent. tint/origin are only
// seeded on first resolve (isNewTexture) so per-instance overrides survive later
// frame-advances instead of being stomped back to the asset defaults every time.
static void ResolveTexture(Elysium::World* world, Elysium::Services::AssetService& assets,
                            Entity entity, const SpriteComponent& spriteComp) {
    Sprite sprite = assets.GetSprite(Path(spriteComp.spriteName));
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
    Texture2D texture = assets.GetTexture(Path(texturePath));
    if (texture.id == 0) return;

    float frameWidth  = (float)texture.width  / (float)sheet.cols;
    float frameHeight = (float)texture.height / (float)sheet.rows;
    size_t col = linearIndex % sheet.cols;
    size_t row = linearIndex / sheet.cols;

    bool isNewTexture = !world->HasComponent<TextureComponent>(entity);
    if (isNewTexture) world->AddComponent<TextureComponent>(entity, TextureComponent{});

    auto& tex = world->GetComponent<TextureComponent>(entity);
    tex.textureName = texturePath;
    tex.sourceRect = { col * frameWidth, row * frameHeight, frameWidth, frameHeight };
    if (isNewTexture) {
        tex.originX = sprite.originX;
        tex.originY = sprite.originY;
    }
}

void SpriteSystem::Update(float deltaTime) {
    auto& assets = Application::GetInstance().GetService<Services::AssetService>();

    world->Query<SpriteComponent>([&](Entity entity, auto& spriteComp) {
        bool frameChanged = !world->HasComponent<TextureComponent>(entity);

        spriteComp.frameElapsed += deltaTime;
        if (spriteComp.frameElapsed >= spriteComp.frameDuration) {
            spriteComp.frameElapsed -= spriteComp.frameDuration;

            auto sprite = assets.GetSprite(Path(spriteComp.spriteName));
            if (sprite.name.empty()) return;

            auto sheetIt = sprite.sheets.find(spriteComp.sheetName);
            if (sheetIt == sprite.sheets.end()) return;
            const SpriteSequence* sequence = nullptr;
            auto seqIt = sheetIt->second.sequences.find(spriteComp.sequenceName);
            if (seqIt != sheetIt->second.sequences.end()) sequence = &seqIt->second;

            if (sequence && !sequence->indices.empty()) {
                // Advance to next frame (loop back to 0 when reaching end)
                spriteComp.sequenceIndex = (spriteComp.sequenceIndex + 1) % sequence->indices.size();
                frameChanged = true;
            }
        }

        if (frameChanged) {
            ResolveTexture(world, assets, entity, spriteComp);
        }
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::SpriteSystem)
