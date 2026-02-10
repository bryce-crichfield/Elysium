#include "Systems/SpriteSystem.h"
#include "Core/Path.h"
#include "Core/SystemRegistry.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Components/SpriteComponent.h"

#include "Core/Application.h"
#include "Services/AssetService.h"

namespace Elysium::Systems {

SpriteSystem::SpriteSystem(Context context) : System(context) {
}

void SpriteSystem::Update(float deltaTime) {
    world->Query<SpriteComponent>([&](Entity entity, auto& spriteComp) {
        auto& assets = Application::GetInstance().GetService<Services::AssetService>();

        spriteComp.frameElapsed += deltaTime;

        if (spriteComp.frameElapsed >= spriteComp.frameDuration) {
            spriteComp.frameElapsed -= spriteComp.frameDuration;  

            auto sprite = assets.GetSprite(Path(spriteComp.spriteName));
            if (sprite.name.empty()) {
                return;
            }

            SpriteSheet sheet = sprite.sheets[spriteComp.sheetName];
            SpriteSequence sequence = sheet.sequences[spriteComp.sequenceName];
            if (sequence.indices.size() > 0) {
                // Advance to next frame (loop back to 0 when reaching end)
                spriteComp.sequenceIndex = (spriteComp.sequenceIndex + 1) % sequence.indices.size();
            }
        }
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::SpriteSystem)
