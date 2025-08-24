#include "Services/CharacterService.h"
#include "Services/CharacterRepository.h"
#include "Services/CharacterUI.h"

namespace Elysium::Services {

CharacterService::CharacterService() {
    name_ = "CharacterService";
}

CharacterService::CharacterService(PersistenceService* persistenceService) {
    name_ = "CharacterService";
    repository_ = std::make_shared<CharacterRepository>(persistenceService);
    ui_ = std::make_unique<CharacterUI>(repository_);
}

CharacterService::~CharacterService() = default;

void CharacterService::Initialize() {
    // Service initialization if needed
}

void CharacterService::Shutdown() {
    // Service cleanup if needed
}

void CharacterService::Update(float deltaTime) {
    // Service update if needed
}

void CharacterService::OnDebugDraw() {
    if (ui_) {
        ui_->Draw();
    }
}

} // namespace Elysium::Services