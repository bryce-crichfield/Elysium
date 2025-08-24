#include "Services/CharacterUI.h"
#include <imgui.h>
#include <algorithm>
#include <fstream>

namespace Elysium::Services {

CharacterUI::CharacterUI(std::shared_ptr<CharacterRepository> repository)
    : repository_(repository) {
    RefreshData();
}

void CharacterUI::Draw() {
        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Characters")) {
                if (ImGui::BeginTabBar("CharacterSubTabs")) {
                    if (ImGui::BeginTabItem("List & Attributes")) {
                        DrawCharacterAttributesTab();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Inventory")) {
                        DrawCharacterInventoryTab();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Spells")) {
                        DrawCharacterSpellsTab();
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Archetypes")) {
                DrawArchetypesTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Items")) {
                DrawItemsTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Spells")) {
                DrawSpellsTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Export")) {
                DrawExportTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        if (!statusMessage_.empty()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", statusMessage_.c_str());
        }
}

// === DATA OPERATIONS ===
void CharacterUI::RefreshData() {
    RefreshCharacters();
    RefreshArchetypes();
    RefreshItems();
    RefreshSpells();
    RefreshCurrentInventory();
    RefreshCurrentSpellbook();
}

void CharacterUI::RefreshCharacters() {
    characters_ = repository_->GetAllCharacters();
}

void CharacterUI::RefreshArchetypes() {
    archetypes_ = repository_->GetAllArchetypes();
}

void CharacterUI::RefreshItems() {
    items_ = repository_->GetAllItems();
}

void CharacterUI::RefreshSpells() {
    spells_ = repository_->GetAllSpells();
}

void CharacterUI::RefreshCurrentInventory() {
    if (currentCharacterId_ != -1) {
        currentInventory_ = repository_->GetCharacterInventory(currentCharacterId_);
    } else {
        currentInventory_.clear();
    }
}

void CharacterUI::RefreshCurrentSpellbook() {
    if (currentCharacterId_ != -1) {
        currentSpellbook_ = repository_->GetCharacterSpells(currentCharacterId_);
    } else {
        currentSpellbook_.clear();
    }
}

// === SELECTIONS ===
void CharacterUI::SelectCharacter(int characterId) {
    currentCharacterId_ = characterId;

    auto it = std::find_if(characters_.begin(), characters_.end(),
        [characterId](const CharacterData& c) { return c.id == characterId; });

    if (it != characters_.end()) {
        characterForm_.LoadFrom(*it, archetypes_);
        RefreshCurrentInventory();
        RefreshCurrentSpellbook();
    }
}

void CharacterUI::SelectArchetype(int archetypeId) {
    currentArchetypeId_ = archetypeId;

    auto it = std::find_if(archetypes_.begin(), archetypes_.end(),
        [archetypeId](const ArchetypeData& a) { return a.id == archetypeId; });

    if (it != archetypes_.end()) {
        archetypeForm_.LoadFrom(*it);
    }
}

void CharacterUI::SelectItem(int itemId) {
    currentItemId_ = itemId;

    auto it = std::find_if(items_.begin(), items_.end(),
        [itemId](const ItemData& i) { return i.id == itemId; });

    if (it != items_.end()) {
        itemForm_.LoadFrom(*it);
    }
}

void CharacterUI::SelectSpell(int spellId) {
    currentSpellId_ = spellId;

    auto it = std::find_if(spells_.begin(), spells_.end(),
        [spellId](const SpellData& s) { return s.id == spellId; });

    if (it != spells_.end()) {
        spellForm_.LoadFrom(*it);
    }
}

void CharacterUI::DrawCharacterAttributesTab() {
    ImGui::Columns(2, "CharacterColumns");

    // Left column - Character list
    ImGui::Text("Characters");
    ImGui::Separator();

    if (ImGui::Button("New Character")) {
        NewCharacter();
    }

    for (const auto& character : characters_) {
        bool selected = (character.id == currentCharacterId_);
        if (ImGui::Selectable(character.name.c_str(), selected)) {
            SelectCharacter(character.id);
        }
    }

    ImGui::NextColumn();

    // Right column - Character form
    ImGui::Text("Character Details");
    ImGui::Separator();

    if (currentCharacterId_ != -1) {
        ImGui::InputText("Name", characterForm_.name, sizeof(characterForm_.name));

        if (ImGui::BeginCombo("Archetype", archetypes_.empty() ? "None" : archetypes_[characterForm_.archetypeIndex].name.c_str())) {
            for (int i = 0; i < archetypes_.size(); ++i) {
                bool selected = (i == characterForm_.archetypeIndex);
                if (ImGui::Selectable(archetypes_[i].name.c_str(), selected)) {
                    characterForm_.archetypeIndex = i;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputInt("Level", &characterForm_.level);
        if (characterForm_.level < 1) characterForm_.level = 1;

        if (ImGui::Button("Save Character")) {
            SaveCharacter();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Character")) {
            DeleteCharacter();
        }

        // Show calculated stats
        if (!archetypes_.empty() && characterForm_.archetypeIndex < archetypes_.size()) {
            auto stats = repository_->CalculateCharacterStats(archetypes_[characterForm_.archetypeIndex].name, characterForm_.level);
            ImGui::Separator();
            ImGui::Text("Calculated Stats:");
            ImGui::Text("Strength: %.1f", stats.strength);
            ImGui::Text("Intelligence: %.1f", stats.intelligence);
            ImGui::Text("Agility: %.1f", stats.agility);
            ImGui::Text("Health: %d", stats.health);
            ImGui::Text("Mana: %d", stats.mana);
        }
    }

    ImGui::Columns(1);
}

void CharacterUI::DrawCharacterInventoryTab() {
    if (currentCharacterId_ == -1) {
        ImGui::Text("Select a character first");
        return;
    }

    ImGui::Columns(2, "InventoryColumns");

    // Left column - Available items
    ImGui::Text("Available Items");
    ImGui::Separator();

    for (const auto& item : items_) {
        if (ImGui::Selectable(item.name.c_str(), item.id == currentItemId_)) {
            SelectItem(item.id);
        }
    }

    if (currentItemId_ != -1 && ImGui::Button("Add to Inventory")) {
        AddItemToInventory();
    }

    ImGui::NextColumn();

    // Right column - Character inventory
    ImGui::Text("Character Inventory");
    ImGui::Separator();

    for (const auto& invItem : currentInventory_) {
        ImGui::Text("%s x%d (%s)", invItem.name.c_str(), invItem.count, invItem.type.c_str());
    }

    ImGui::Columns(1);
}

void CharacterUI::DrawCharacterSpellsTab() {
    if (currentCharacterId_ == -1) {
        ImGui::Text("Select a character first");
        return;
    }

    ImGui::Columns(2, "SpellsColumns");

    // Left column - Available spells
    ImGui::Text("Available Spells");
    ImGui::Separator();

    for (const auto& spell : spells_) {
        if (ImGui::Selectable(spell.name.c_str(), spell.id == currentSpellId_)) {
            SelectSpell(spell.id);
        }
    }

    if (currentSpellId_ != -1 && ImGui::Button("Add to Spellbook")) {
        AddSpellToSpellbook();
    }

    ImGui::NextColumn();

    // Right column - Character spellbook
    ImGui::Text("Character Spellbook");
    ImGui::Separator();

    for (const auto& spell : currentSpellbook_) {
        ImGui::Text("%s (%s: %d)", spell.name.c_str(), spell.cost_type.c_str(), spell.cost_amount);
    }

    ImGui::Columns(1);
}

void CharacterUI::DrawArchetypesTab() {
    ImGui::Columns(2, "ArchetypeColumns");

    // Left column - Archetype list
    ImGui::Text("Archetypes");
    ImGui::Separator();

    if (ImGui::Button("New Archetype")) {
        NewArchetype();
    }

    for (const auto& archetype : archetypes_) {
        bool selected = (archetype.id == currentArchetypeId_);
        if (ImGui::Selectable(archetype.name.c_str(), selected)) {
            SelectArchetype(archetype.id);
        }
    }

    ImGui::NextColumn();

    // Right column - Archetype form
    ImGui::Text("Archetype Details");
    ImGui::Separator();

    if (currentArchetypeId_ != -1) {
        ImGui::InputText("Name", archetypeForm_.name, sizeof(archetypeForm_.name));

        ImGui::Text("Base Stats:");
        ImGui::InputInt("Strength", &archetypeForm_.strBase);
        ImGui::InputInt("Intelligence", &archetypeForm_.intBase);
        ImGui::InputInt("Agility", &archetypeForm_.agiBase);

        ImGui::Text("Growth Rates:");
        ImGui::InputFloat("Str Growth", &archetypeForm_.strGrowth, 0.1f, 1.0f, "%.1f");
        ImGui::InputFloat("Int Growth", &archetypeForm_.intGrowth, 0.1f, 1.0f, "%.1f");
        ImGui::InputFloat("Agi Growth", &archetypeForm_.agiGrowth, 0.1f, 1.0f, "%.1f");

        if (ImGui::Button("Save Archetype")) {
            SaveArchetype();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Archetype")) {
            DeleteArchetype();
        }
    }

    ImGui::Columns(1);
}

void CharacterUI::DrawItemsTab() {
    ImGui::Columns(2, "ItemColumns");

    // Left column - Item list
    ImGui::Text("Items");
    ImGui::Separator();

    if (ImGui::Button("New Item")) {
        NewItem();
    }

    for (const auto& item : items_) {
        bool selected = (item.id == currentItemId_);
        if (ImGui::Selectable(item.name.c_str(), selected)) {
            SelectItem(item.id);
        }
    }

    ImGui::NextColumn();

    // Right column - Item form
    ImGui::Text("Item Details");
    ImGui::Separator();

    if (currentItemId_ != -1) {
        ImGui::InputText("Name", itemForm_.name, sizeof(itemForm_.name));
        ImGui::InputText("Type", itemForm_.type, sizeof(itemForm_.type));

        ImGui::Text("Stats:");
        ImGui::InputInt("Health", &itemForm_.health);
        ImGui::InputInt("Weapon Damage", &itemForm_.weaponDamage);
        ImGui::InputInt("Spell Power", &itemForm_.spellPower);
        ImGui::InputInt("Armor", &itemForm_.armor);
        ImGui::InputInt("Ward", &itemForm_.ward);
        ImGui::InputInt("Mana", &itemForm_.mana);
        ImGui::InputInt("Stamina", &itemForm_.stamina);

        if (ImGui::Button("Save Item")) {
            SaveItem();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Item")) {
            DeleteItem();
        }
    }

    ImGui::Columns(1);
}

void CharacterUI::DrawSpellsTab() {
    ImGui::Columns(2, "SpellColumns");

    // Left column - Spell list
    ImGui::Text("Spells");
    ImGui::Separator();

    if (ImGui::Button("New Spell")) {
        NewSpell();
    }

    for (const auto& spell : spells_) {
        bool selected = (spell.id == currentSpellId_);
        if (ImGui::Selectable(spell.name.c_str(), selected)) {
            SelectSpell(spell.id);
        }
    }

    ImGui::NextColumn();

    // Right column - Spell form
    ImGui::Text("Spell Details");
    ImGui::Separator();

    if (currentSpellId_ != -1) {
        ImGui::InputText("Name", spellForm_.name, sizeof(spellForm_.name));
        ImGui::InputText("Cost Type", spellForm_.costType, sizeof(spellForm_.costType));
        ImGui::InputInt("Cost Amount", &spellForm_.costAmount);

        if (ImGui::Button("Save Spell")) {
            SaveSpell();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Spell")) {
            DeleteSpell();
        }
    }

    ImGui::Columns(1);
}

void CharacterUI::DrawExportTab() {
    ImGui::Text("Export all data to JSON file");
    ImGui::Separator();

    if (ImGui::Button("Export to JSON")) {
        ExportToJson();
    }
}

// === CRUD OPERATIONS ===
void CharacterUI::SaveCharacter() {
    if (currentCharacterId_ == -1) return;

    if (!archetypes_.empty() && characterForm_.archetypeIndex < archetypes_.size()) {
        int archetypeId = archetypes_[characterForm_.archetypeIndex].id;
        if (repository_->UpdateCharacter(currentCharacterId_, characterForm_.name, archetypeId, characterForm_.level)) {
            statusMessage_ = "Character saved successfully";
            RefreshCharacters();
        } else {
            statusMessage_ = "Failed to save character";
        }
    }
}

void CharacterUI::SaveArchetype() {
    if (currentArchetypeId_ == -1) return;

    if (repository_->UpdateArchetype(currentArchetypeId_, archetypeForm_.name,
                                   archetypeForm_.strBase, archetypeForm_.intBase, archetypeForm_.agiBase,
                                   archetypeForm_.strGrowth, archetypeForm_.intGrowth, archetypeForm_.agiGrowth)) {
        statusMessage_ = "Archetype saved successfully";
        RefreshArchetypes();
    } else {
        statusMessage_ = "Failed to save archetype";
    }
}

void CharacterUI::SaveItem() {
    if (currentItemId_ == -1) return;

    if (repository_->UpdateItem(currentItemId_, itemForm_.name, itemForm_.type,
                              itemForm_.health, itemForm_.weaponDamage, itemForm_.spellPower,
                              itemForm_.armor, itemForm_.ward, itemForm_.mana, itemForm_.stamina)) {
        statusMessage_ = "Item saved successfully";
        RefreshItems();
    } else {
        statusMessage_ = "Failed to save item";
    }
}

void CharacterUI::SaveSpell() {
    if (currentSpellId_ == -1) return;

    if (repository_->UpdateSpell(currentSpellId_, spellForm_.name, spellForm_.costType, spellForm_.costAmount)) {
        statusMessage_ = "Spell saved successfully";
        RefreshSpells();
    } else {
        statusMessage_ = "Failed to save spell";
    }
}

void CharacterUI::DeleteCharacter() {
    if (currentCharacterId_ == -1) return;

    if (repository_->DeleteCharacter(currentCharacterId_)) {
        statusMessage_ = "Character deleted successfully";
        currentCharacterId_ = -1;
        characterForm_.Clear();
        RefreshCharacters();
        currentInventory_.clear();
        currentSpellbook_.clear();
    } else {
        statusMessage_ = "Failed to delete character";
    }
}

void CharacterUI::DeleteArchetype() {
    if (currentArchetypeId_ == -1) return;

    if (repository_->DeleteArchetype(currentArchetypeId_)) {
        statusMessage_ = "Archetype deleted successfully";
        currentArchetypeId_ = -1;
        archetypeForm_.Clear();
        RefreshArchetypes();
    } else {
        statusMessage_ = "Failed to delete archetype";
    }
}

void CharacterUI::DeleteItem() {
    if (currentItemId_ == -1) return;

    if (repository_->DeleteItem(currentItemId_)) {
        statusMessage_ = "Item deleted successfully";
        currentItemId_ = -1;
        itemForm_.Clear();
        RefreshItems();
    } else {
        statusMessage_ = "Failed to delete item";
    }
}

void CharacterUI::DeleteSpell() {
    if (currentSpellId_ == -1) return;

    if (repository_->DeleteSpell(currentSpellId_)) {
        statusMessage_ = "Spell deleted successfully";
        currentSpellId_ = -1;
        spellForm_.Clear();
        RefreshSpells();
    } else {
        statusMessage_ = "Failed to delete spell";
    }
}

void CharacterUI::NewCharacter() {
    characterForm_.Clear();
    if (!archetypes_.empty()) {
        int newId = repository_->CreateCharacter("New Character", archetypes_[0].id, 10);
        if (newId != -1) {
            statusMessage_ = "New character created";
            RefreshCharacters();
            SelectCharacter(newId);
        } else {
            statusMessage_ = "Failed to create character";
        }
    } else {
        statusMessage_ = "Create an archetype first";
    }
}

void CharacterUI::NewArchetype() {
    archetypeForm_.Clear();
    int newId = repository_->CreateArchetype("New Archetype", 10, 10, 10, 2.0f, 2.0f, 2.0f);
    if (newId != -1) {
        statusMessage_ = "New archetype created";
        RefreshArchetypes();
        SelectArchetype(newId);
    } else {
        statusMessage_ = "Failed to create archetype";
    }
}

void CharacterUI::NewItem() {
    itemForm_.Clear();
    int newId = repository_->CreateItem("New Item", "weapon", 0, 0, 0, 0, 0, 0, 0);
    if (newId != -1) {
        statusMessage_ = "New item created";
        RefreshItems();
        SelectItem(newId);
    } else {
        statusMessage_ = "Failed to create item";
    }
}

void CharacterUI::NewSpell() {
    spellForm_.Clear();
    int newId = repository_->CreateSpell("New Spell", "mana", 10);
    if (newId != -1) {
        statusMessage_ = "New spell created";
        RefreshSpells();
        SelectSpell(newId);
    } else {
        statusMessage_ = "Failed to create spell";
    }
}

// === HELPER METHODS ===
void CharacterUI::AddItemToInventory() {
    if (currentCharacterId_ == -1 || currentItemId_ == -1) return;

    if (repository_->AddItemToInventory(currentCharacterId_, currentItemId_)) {
        statusMessage_ = "Item added to inventory";
        RefreshCurrentInventory();
    } else {
        statusMessage_ = "Failed to add item to inventory";
    }
}

void CharacterUI::AddSpellToSpellbook() {
    if (currentCharacterId_ == -1 || currentSpellId_ == -1) return;

    if (repository_->AddSpellToSpellbook(currentCharacterId_, currentSpellId_)) {
        statusMessage_ = "Spell added to spellbook";
        RefreshCurrentSpellbook();
    } else {
        statusMessage_ = "Failed to add spell to spellbook";
    }
}

void CharacterUI::ExportToJson() {
    try {


        statusMessage_ = "Data exported to character_export.json";
    } catch (const std::exception& e) {
        statusMessage_ = "Export failed: " + std::string(e.what());
    }
}

} // namespace Elysium::Services
