#pragma once

#include <memory>

namespace Elysium::Services {

class PersistenceService;
class CharacterRepository;
class CharacterUI;

// Clean wrapper for the character editor
class CharacterService {
public:
    CharacterService(PersistenceService* persistenceService);
    ~CharacterService();

    void Draw();

private:
    std::shared_ptr<CharacterRepository> repository_;
    std::unique_ptr<CharacterUI> ui_;
};

} // namespace Elysium::Services