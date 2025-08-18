#include "Services/CharacterService.h"
#include "Services/CharacterRepository.h"
#include "Services/CharacterUI.h"

namespace Elysium::Services {

CharacterService::CharacterService(PersistenceService* persistenceService) {
    repository_ = std::make_shared<CharacterRepository>(persistenceService);
    ui_ = std::make_unique<CharacterUI>(repository_);
}

CharacterService::~CharacterService() = default;

void CharacterService::Draw() {
    ui_->Draw();
}

} // namespace Elysium::Services