#pragma once

#include "Service.h"
#include <memory>

namespace Elysium::Services {

class PersistenceService;
class CharacterRepository;
class CharacterUI;

// Clean wrapper for the character editor
class CharacterService : public Elysium::Service {
public:
    CharacterService();
    CharacterService(PersistenceService* persistenceService);
    ~CharacterService();

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void OnDebugDraw() override;

private:
    std::shared_ptr<CharacterRepository> repository_;
    std::unique_ptr<CharacterUI> ui_;
};

} // namespace Elysium::Services