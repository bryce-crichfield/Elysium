#pragma once

namespace Elysium::Components {

struct CharacterComponent {
    int character_id = -1;
    
    CharacterComponent() = default;
    CharacterComponent(int characterId) : character_id(characterId) {}
};

} // namespace Elysium::Components