#include "Services/CharacterService.h"
#include "Services/PersistenceService.h"
#include "Services/LogService.h"
#include "imgui.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm>
#include <cstring>

namespace Elysium::Services {

CharacterService::CharacterService(PersistenceService* persistenceService)
    : persistenceService_(persistenceService)
    , currentCharacterId_(-1)
    , currentArchetypeId_(-1)
    , currentItemId_(-1)
    , currentSpellId_(-1)
    , selectedTab_(0)
    , characterSubTab_(0)
    , characterLevel_(10)
    , selectedArchetypeIndex_(0)
    , selectedItemIndex_(0)
    , selectedSpellIndex_(0)
    , strBase_(10)
    , intBase_(10)
    , agiBase_(10)
    , strGrowth_(2.0f)
    , intGrowth_(2.0f)
    , agiGrowth_(2.0f)
    , itemHealth_(0)
    , itemWeaponDamage_(0)
    , itemSpellPower_(0)
    , itemArmor_(0)
    , spellCostAmount_(10)
{
    memset(characterName_, 0, sizeof(characterName_));
    memset(archetypeName_, 0, sizeof(archetypeName_));
    memset(itemName_, 0, sizeof(itemName_));
    memset(spellName_, 0, sizeof(spellName_));
    memset(itemType_, 0, sizeof(itemType_));
    memset(spellCostType_, 0, sizeof(spellCostType_));

    strcpy(itemType_, "weapon");
    strcpy(spellCostType_, "mana");

    RefreshData();
}

CharacterService::~CharacterService() {
}

void CharacterService::Draw() {
    if (ImGui::BeginTabBar("PersistenceServiceTabs")) {
        if (ImGui::BeginTabItem("Characters")) {
            // Character Editor (top section)
            ImGui::Text("Character Editor");
            ImGui::Separator();
            DrawCharactersTab();
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
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", statusMessage_.c_str());
    }
}

void CharacterService::RefreshData() {
    RefreshArchetypes();
    RefreshItems();
    RefreshSpells();
    RefreshCharacters();
    RefreshCurrentInventory();
    RefreshCurrentSpellbook();
}

void CharacterService::RefreshArchetypes() {
    archetypes_.clear();

    if (!persistenceService_ || !persistenceService_->GetDatabase()) return;

    try {
        SQLite::Statement query(*persistenceService_->GetDatabase(), "SELECT * FROM archetypes ORDER BY name");

        while (query.executeStep()) {
            ArchetypeData arch;
            arch.id = query.getColumn(0).getInt();
            arch.name = query.getColumn(1).getText();
            arch.str_base = query.getColumn(2).getInt();
            arch.int_base = query.getColumn(3).getInt();
            arch.agi_base = query.getColumn(4).getInt();
            arch.str_growth = query.getColumn(5).getDouble();
            arch.int_growth = query.getColumn(6).getDouble();
            arch.agi_growth = query.getColumn(7).getDouble();
            archetypes_.push_back(arch);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("CharacterService", "Error refreshing archetypes: " + std::string(e.what()));
    }
}

void CharacterService::RefreshItems() {
    items_.clear();

    if (!persistenceService_ || !persistenceService_->GetDatabase()) return;

    try {
        SQLite::Statement query(*persistenceService_->GetDatabase(), "SELECT * FROM items ORDER BY name");

        while (query.executeStep()) {
            ItemData item;
            item.id = query.getColumn(0).getInt();
            item.name = query.getColumn(1).getText();
            item.type = query.getColumn(2).getText();
            item.health = query.getColumn(3).getInt();
            item.weapon_damage = query.getColumn(4).getInt();
            item.spell_power = query.getColumn(5).getInt();
            item.armor = query.getColumn(6).getInt();
            item.ward = query.getColumn(7).getInt();
            item.mana = query.getColumn(8).getInt();
            item.stamina = query.getColumn(9).getInt();
            items_.push_back(item);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("CharacterService", "Error refreshing items: " + std::string(e.what()));
    }
}

void CharacterService::RefreshSpells() {
    spells_.clear();

    if (!persistenceService_ || !persistenceService_->GetDatabase()) return;

    try {
        SQLite::Statement query(*persistenceService_->GetDatabase(), "SELECT * FROM spells ORDER BY name");

        while (query.executeStep()) {
            SpellData spell;
            spell.id = query.getColumn(0).getInt();
            spell.name = query.getColumn(1).getText();
            spell.cost_type = query.getColumn(2).getText();
            spell.cost_amount = query.getColumn(3).getInt();
            spells_.push_back(spell);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("CharacterService", "Error refreshing spells: " + std::string(e.what()));
    }
}

void CharacterService::RefreshCharacters() {
    characters_.clear();

    if (!persistenceService_ || !persistenceService_->GetDatabase()) return;

    try {
        SQLite::Statement query(*persistenceService_->GetDatabase(),
            "SELECT c.id, c.name, c.archetype_id, a.name, c.level "
            "FROM characters c "
            "JOIN archetypes a ON c.archetype_id = a.id "
            "ORDER BY c.name");

        while (query.executeStep()) {
            CharacterData config;
            config.id = query.getColumn(0).getInt();
            config.name = query.getColumn(1).getText();
            config.archetype_id = query.getColumn(2).getInt();
            config.archetype_name = query.getColumn(3).getText();
            config.level = query.getColumn(4).getInt();
            characters_.push_back(config);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("CharacterService", "Error refreshing characters: " + std::string(e.what()));
    }
}

void CharacterService::RefreshCurrentInventory() {
    currentInventory_.clear();

    if (currentCharacterId_ == -1 || !persistenceService_ || !persistenceService_->GetDatabase()) return;

    try {
        SQLite::Statement query(*persistenceService_->GetDatabase(),
            "SELECT i.name, i.type, COUNT(*) as count "
            "FROM inventory_items ii "
            "JOIN inventories inv ON ii.inventory_id = inv.id "
            "JOIN items i ON ii.item_id = i.id "
            "WHERE inv.character_id = ? "
            "GROUP BY i.id, i.name, i.type "
            "ORDER BY i.name");

        query.bind(1, currentCharacterId_);

        while (query.executeStep()) {
            InventoryItemData item;
            item.name = query.getColumn(0).getText();
            item.type = query.getColumn(1).getText();
            item.count = query.getColumn(2).getInt();
            currentInventory_.push_back(item);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("CharacterService", "Error refreshing inventory: " + std::string(e.what()));
    }
}

void CharacterService::RefreshCurrentSpellbook() {
    currentSpellbook_.clear();

    if (currentCharacterId_ == -1 || !persistenceService_ || !persistenceService_->GetDatabase()) return;

    try {
        SQLite::Statement query(*persistenceService_->GetDatabase(),
            "SELECT s.id, s.name, s.cost_type, s.cost_amount "
            "FROM spellbook_spells ss "
            "JOIN spellbooks sb ON ss.spellbook_id = sb.id "
            "JOIN spells s ON ss.spell_id = s.id "
            "WHERE sb.character_id = ? "
            "ORDER BY s.name");

        query.bind(1, currentCharacterId_);

        while (query.executeStep()) {
            SpellData spell;
            spell.id = query.getColumn(0).getInt();
            spell.name = query.getColumn(1).getText();
            spell.cost_type = query.getColumn(2).getText();
            spell.cost_amount = query.getColumn(3).getInt();
            currentSpellbook_.push_back(spell);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("CharacterService", "Error refreshing spellbook: " + std::string(e.what()));
    }
}

void CharacterService::DrawCharactersTab() {
    // Character Editor Sub-tabs
    if (ImGui::BeginTabBar("CharacterEditorTabs")) {
        // Attributes tab
        if (ImGui::BeginTabItem("Attributes")) {
            DrawCharacterAttributesTab();
            ImGui::EndTabItem();
        }

        // Inventory tab (disabled if no character selected)
        bool hasCharacter = currentCharacterId_ != -1;
        if (!hasCharacter) {
            ImGui::BeginDisabled();
        }
        if (ImGui::BeginTabItem("Inventory")) {
            DrawCharacterInventoryTab();
            ImGui::EndTabItem();
        }
        if (!hasCharacter) {
            ImGui::EndDisabled();
        }

        // Spells tab (disabled if no character selected)
        if (!hasCharacter) {
            ImGui::BeginDisabled();
        }
        if (ImGui::BeginTabItem("Spells")) {
            DrawCharacterSpellsTab();
            ImGui::EndTabItem();
        }
        if (!hasCharacter) {
            ImGui::EndDisabled();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Characters List (bottom section)
    ImGui::Text("Characters List");
    ImGui::Separator();

    if (ImGui::BeginTable("CharacterTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Archetype");
        ImGui::TableSetupColumn("Level");
        ImGui::TableHeadersRow();

        for (const auto& character : characters_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(std::to_string(character.id).c_str(), currentCharacterId_ == character.id, ImGuiSelectableFlags_SpanAllColumns)) {
                SelectCharacter(character.id);
            }
            ImGui::TableNextColumn();
            ImGui::Text("%s", character.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", character.archetype_name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", character.level);
        }

        ImGui::EndTable();
    }
}

void CharacterService::DrawCharacterAttributesTab() {

    if (ImGui::InputText("Name", characterName_, sizeof(characterName_))) {
        // Auto-save when name changes (only if character is selected)
        if (currentCharacterId_ != -1) {
            SaveCharacter();
        }
    }

    // Archetype combo
    if (ImGui::BeginCombo("Archetype", selectedArchetypeIndex_ < archetypes_.size() ? archetypes_[selectedArchetypeIndex_].name.c_str() : "None")) {
        for (size_t i = 0; i < archetypes_.size(); ++i) {
            bool isSelected = (selectedArchetypeIndex_ == i);
            if (ImGui::Selectable(archetypes_[i].name.c_str(), isSelected)) {
                selectedArchetypeIndex_ = i;
                UpdateCharacterPreview();
                // Auto-save when archetype changes (only if character is selected)
                if (currentCharacterId_ != -1) {
                    SaveCharacter();
                }
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::SliderInt("Level", &characterLevel_, 1, 50)) {
        UpdateCharacterPreview();
        // Auto-save when level changes (only if character is selected)
        if (currentCharacterId_ != -1) {
            SaveCharacter();
        }
    }

    ImGui::Separator();

    if (currentCharacterId_ == -1) {
        // No character selected - show Create button
        if (ImGui::Button("Create Character")) {
            SaveCharacter();  // This will insert new character since currentCharacterId_ == -1
        }
    } else {
        // Character selected - show Deselect button
        if (ImGui::Button("Deselect Character")) {
            NewCharacter();  // Clear selection and reset form
        }

        ImGui::SameLine();
        if (ImGui::Button("Delete Character")) {
            ImGui::OpenPopup("Delete Character?");
        }

        if (ImGui::BeginPopupModal("Delete Character?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete this character?\nThis action cannot be undone!");
            ImGui::Separator();

            if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                DeleteCharacter();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::Separator();
    ImGui::Text("Preview Stats:");

    float str, intel, agi;
    int health, mana;
    if (selectedArchetypeIndex_ < archetypes_.size()) {
        CalculateStats(archetypes_[selectedArchetypeIndex_].name, characterLevel_, str, intel, agi, health, mana);
        ImGui::Text("STR: %.1f", str);
        ImGui::Text("INT: %.1f", intel);
        ImGui::Text("AGI: %.1f", agi);
        ImGui::Text("Health: %d", health);
        ImGui::Text("Mana: %d", mana);
    }
}

void CharacterService::DrawCharacterInventoryTab() {
    ImGui::Text("Inventory Management");
    ImGui::Separator();

    // Current inventory display
    ImGui::Text("Current Inventory:");
    if (ImGui::BeginChild("InventoryList", ImVec2(0, 200))) {
        for (const auto& item : currentInventory_) {
            ImGui::Text("%s (%s) x%d", item.name.c_str(), item.type.c_str(), item.count);
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Add item to inventory
    ImGui::Text("Add Item:");
    if (ImGui::BeginCombo("##AddItem", selectedItemIndex_ < items_.size() ? items_[selectedItemIndex_].name.c_str() : "Select Item...")) {
        for (size_t i = 0; i < items_.size(); ++i) {
            bool isSelected = (selectedItemIndex_ == i);
            if (ImGui::Selectable(items_[i].name.c_str(), isSelected)) {
                selectedItemIndex_ = i;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add to Inventory")) {
        AddItemToInventory();
    }
}

void CharacterService::DrawCharacterSpellsTab() {
    ImGui::Text("Spellbook Management");
    ImGui::Separator();

    // Current spellbook display
    ImGui::Text("Current Spellbook:");
    if (ImGui::BeginChild("SpellbookList", ImVec2(0, 200))) {
        for (const auto& spell : currentSpellbook_) {
            ImGui::Text("%s (%s: %d)", spell.name.c_str(), spell.cost_type.c_str(), spell.cost_amount);
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Add spell to spellbook
    ImGui::Text("Add Spell:");
    if (ImGui::BeginCombo("##AddSpell", selectedSpellIndex_ < spells_.size() ? spells_[selectedSpellIndex_].name.c_str() : "Select Spell...")) {
        for (size_t i = 0; i < spells_.size(); ++i) {
            bool isSelected = (selectedSpellIndex_ == i);
            if (ImGui::Selectable(spells_[i].name.c_str(), isSelected)) {
                selectedSpellIndex_ = i;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add to Spellbook")) {
        AddSpellToSpellbook();
    }
}

void CharacterService::DrawArchetypesTab() {
    // Archetype Editor (top section)
    ImGui::Text("Archetype Editor");
    ImGui::Separator();

    if (ImGui::InputText("Name", archetypeName_, sizeof(archetypeName_))) {
        if (currentArchetypeId_ != -1) SaveArchetype();
    }
    if (ImGui::InputInt("Base STR", &strBase_)) {
        if (currentArchetypeId_ != -1) SaveArchetype();
    }
    if (ImGui::InputInt("Base INT", &intBase_)) {
        if (currentArchetypeId_ != -1) SaveArchetype();
    }
    if (ImGui::InputInt("Base AGI", &agiBase_)) {
        if (currentArchetypeId_ != -1) SaveArchetype();
    }
    if (ImGui::InputFloat("STR Growth", &strGrowth_)) {
        if (currentArchetypeId_ != -1) SaveArchetype();
    }
    if (ImGui::InputFloat("INT Growth", &intGrowth_)) {
        if (currentArchetypeId_ != -1) SaveArchetype();
    }
    if (ImGui::InputFloat("AGI Growth", &agiGrowth_)) {
        if (currentArchetypeId_ != -1) SaveArchetype();
    }

    ImGui::Separator();

    if (currentArchetypeId_ == -1) {
        // No archetype selected - show Create button
        if (ImGui::Button("Create Archetype")) {
            SaveArchetype();  // This will insert new archetype since currentArchetypeId_ == -1
        }
    } else {
        // Archetype selected - show Deselect button
        if (ImGui::Button("Deselect Archetype")) {
            NewArchetype();  // Clear selection and reset form
        }

        ImGui::SameLine();
        if (ImGui::Button("Delete Archetype")) {
            ImGui::OpenPopup("Delete Archetype?");
        }

        if (ImGui::BeginPopupModal("Delete Archetype?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete this archetype?\nThis action cannot be undone!");
            ImGui::Separator();

            if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                DeleteArchetype();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Archetypes List (bottom section)
    ImGui::Text("Archetypes List");
    ImGui::Separator();

    if (ImGui::BeginTable("ArchetypeTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("STR Base");
        ImGui::TableSetupColumn("INT Base");
        ImGui::TableSetupColumn("AGI Base");
        ImGui::TableSetupColumn("STR Growth");
        ImGui::TableSetupColumn("INT Growth");
        ImGui::TableSetupColumn("AGI Growth");
        ImGui::TableHeadersRow();

        for (const auto& arch : archetypes_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(std::to_string(arch.id).c_str(), currentArchetypeId_ == arch.id, ImGuiSelectableFlags_SpanAllColumns)) {
                SelectArchetype(arch.id);
            }
            ImGui::TableNextColumn();
            ImGui::Text("%s", arch.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", arch.str_base);
            ImGui::TableNextColumn();
            ImGui::Text("%d", arch.int_base);
            ImGui::TableNextColumn();
            ImGui::Text("%d", arch.agi_base);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", arch.str_growth);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", arch.int_growth);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", arch.agi_growth);
        }

        ImGui::EndTable();
    }
}

void CharacterService::DrawItemsTab() {
    // Item Editor (top section)
    ImGui::Text("Item Editor");
    ImGui::Separator();

    if (ImGui::InputText("Name", itemName_, sizeof(itemName_))) {
        if (currentItemId_ != -1) SaveItem();
    }

    const char* itemTypes[] = { "weapon", "armor", "accessory" };
    if (ImGui::BeginCombo("Type", itemType_)) {
        for (int i = 0; i < 3; ++i) {
            bool isSelected = (strcmp(itemType_, itemTypes[i]) == 0);
            if (ImGui::Selectable(itemTypes[i], isSelected)) {
                strcpy(itemType_, itemTypes[i]);
                if (currentItemId_ != -1) SaveItem();
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::InputInt("Health", &itemHealth_)) {
        if (currentItemId_ != -1) SaveItem();
    }
    if (ImGui::InputInt("Weapon Damage", &itemWeaponDamage_)) {
        if (currentItemId_ != -1) SaveItem();
    }
    if (ImGui::InputInt("Spell Power", &itemSpellPower_)) {
        if (currentItemId_ != -1) SaveItem();
    }
    if (ImGui::InputInt("Armor", &itemArmor_)) {
        if (currentItemId_ != -1) SaveItem();
    }

    ImGui::Separator();

    if (currentItemId_ == -1) {
        // No item selected - show Create button
        if (ImGui::Button("Create Item")) {
            SaveItem();  // This will insert new item since currentItemId_ == -1
        }
    } else {
        // Item selected - show Deselect button
        if (ImGui::Button("Deselect Item")) {
            NewItem();  // Clear selection and reset form
        }

        ImGui::SameLine();
        if (ImGui::Button("Delete Item")) {
            ImGui::OpenPopup("Delete Item?");
        }

        if (ImGui::BeginPopupModal("Delete Item?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete this item?\nThis action cannot be undone!");
            ImGui::Separator();

            if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                DeleteItem();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Items List (bottom section)
    ImGui::Text("Items List");
    ImGui::Separator();

    if (ImGui::BeginTable("ItemTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Health");
        ImGui::TableSetupColumn("Weapon Dmg");
        ImGui::TableSetupColumn("Spell Power");
        ImGui::TableSetupColumn("Armor");
        ImGui::TableHeadersRow();

        for (const auto& item : items_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(std::to_string(item.id).c_str(), currentItemId_ == item.id, ImGuiSelectableFlags_SpanAllColumns)) {
                SelectItem(item.id);
            }
            ImGui::TableNextColumn();
            ImGui::Text("%s", item.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", item.type.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", item.health);
            ImGui::TableNextColumn();
            ImGui::Text("%d", item.weapon_damage);
            ImGui::TableNextColumn();
            ImGui::Text("%d", item.spell_power);
            ImGui::TableNextColumn();
            ImGui::Text("%d", item.armor);
        }

        ImGui::EndTable();
    }
}

void CharacterService::DrawSpellsTab() {
    // Spell Editor (top section)
    ImGui::Text("Spell Editor");
    ImGui::Separator();

    if (ImGui::InputText("Name", spellName_, sizeof(spellName_))) {
        if (currentSpellId_ != -1) SaveSpell();
    }

    const char* costTypes[] = { "mana", "stamina" };
    if (ImGui::BeginCombo("Cost Type", spellCostType_)) {
        for (int i = 0; i < 2; ++i) {
            bool isSelected = (strcmp(spellCostType_, costTypes[i]) == 0);
            if (ImGui::Selectable(costTypes[i], isSelected)) {
                strcpy(spellCostType_, costTypes[i]);
                if (currentSpellId_ != -1) SaveSpell();
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::InputInt("Cost Amount", &spellCostAmount_)) {
        if (currentSpellId_ != -1) SaveSpell();
    }

    ImGui::Separator();

    if (currentSpellId_ == -1) {
        // No spell selected - show Create button
        if (ImGui::Button("Create Spell")) {
            SaveSpell();  // This will insert new spell since currentSpellId_ == -1
        }
    } else {
        // Spell selected - show Deselect button
        if (ImGui::Button("Deselect Spell")) {
            NewSpell();  // Clear selection and reset form
        }

        ImGui::SameLine();
        if (ImGui::Button("Delete Spell")) {
            ImGui::OpenPopup("Delete Spell?");
        }

        if (ImGui::BeginPopupModal("Delete Spell?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete this spell?\nThis action cannot be undone!");
            ImGui::Separator();

            if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                DeleteSpell();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Spells List (bottom section)
    ImGui::Text("Spells List");
    ImGui::Separator();

    if (ImGui::BeginTable("SpellTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Cost Type");
        ImGui::TableSetupColumn("Cost Amount");
        ImGui::TableHeadersRow();

        for (const auto& spell : spells_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(std::to_string(spell.id).c_str(), currentSpellId_ == spell.id, ImGuiSelectableFlags_SpanAllColumns)) {
                SelectSpell(spell.id);
            }
            ImGui::TableNextColumn();
            ImGui::Text("%s", spell.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", spell.cost_type.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", spell.cost_amount);
        }

        ImGui::EndTable();
    }
}

void CharacterService::DrawExportTab() {
    ImGui::Text("Export Game Data");
    ImGui::Separator();

    if (ImGui::Button("Export to JSON")) {
        ExportToJson();
    }

    if (ImGui::Button("Refresh All Data")) {
        RefreshData();
        statusMessage_ = "Data refreshed successfully!";
    }
}

void CharacterService::SelectCharacter(int characterId) {
    currentCharacterId_ = characterId;

    auto it = std::find_if(characters_.begin(), characters_.end(),
                          [characterId](const CharacterData& character) { return character.id == characterId; });

    if (it != characters_.end()) {
        strcpy(characterName_, it->name.c_str());
        characterLevel_ = it->level;

        // Find archetype index
        auto archIt = std::find_if(archetypes_.begin(), archetypes_.end(),
                                  [&](const ArchetypeData& arch) { return arch.name == it->archetype_name; });
        if (archIt != archetypes_.end()) {
            selectedArchetypeIndex_ = std::distance(archetypes_.begin(), archIt);
        }

        UpdateCharacterPreview();
        RefreshCurrentInventory();
        RefreshCurrentSpellbook();
    }
}

void CharacterService::SelectArchetype(int archetypeId) {
    currentArchetypeId_ = archetypeId;

    auto it = std::find_if(archetypes_.begin(), archetypes_.end(),
                          [archetypeId](const ArchetypeData& arch) { return arch.id == archetypeId; });

    if (it != archetypes_.end()) {
        strcpy(archetypeName_, it->name.c_str());
        strBase_ = it->str_base;
        intBase_ = it->int_base;
        agiBase_ = it->agi_base;
        strGrowth_ = it->str_growth;
        intGrowth_ = it->int_growth;
        agiGrowth_ = it->agi_growth;
    }
}

void CharacterService::SelectItem(int itemId) {
    currentItemId_ = itemId;

    auto it = std::find_if(items_.begin(), items_.end(),
                          [itemId](const ItemData& item) { return item.id == itemId; });

    if (it != items_.end()) {
        strcpy(itemName_, it->name.c_str());
        strcpy(itemType_, it->type.c_str());
        itemHealth_ = it->health;
        itemWeaponDamage_ = it->weapon_damage;
        itemSpellPower_ = it->spell_power;
        itemArmor_ = it->armor;
    }
}

void CharacterService::SelectSpell(int spellId) {
    currentSpellId_ = spellId;

    auto it = std::find_if(spells_.begin(), spells_.end(),
                          [spellId](const SpellData& spell) { return spell.id == spellId; });

    if (it != spells_.end()) {
        strcpy(spellName_, it->name.c_str());
        strcpy(spellCostType_, it->cost_type.c_str());
        spellCostAmount_ = it->cost_amount;
    }
}

void CharacterService::SaveCharacter() {
    if (!persistenceService_ || !persistenceService_->GetDatabase()) {
        statusMessage_ = "Error: Database not available";
        return;
    }

    if (strlen(characterName_) == 0 || selectedArchetypeIndex_ >= archetypes_.size()) {
        statusMessage_ = "Error: Please enter a name and select an archetype";
        return;
    }

    try {
        int archetypeId = archetypes_[selectedArchetypeIndex_].id;

        if (currentCharacterId_ != -1) {
            // Update existing character
            SQLite::Statement stmt(*persistenceService_->GetDatabase(),
                "UPDATE characters SET name = ?, archetype_id = ?, level = ? WHERE id = ?");
            stmt.bind(1, std::string(characterName_));
            stmt.bind(2, archetypeId);
            stmt.bind(3, characterLevel_);
            stmt.bind(4, currentCharacterId_);
            stmt.exec();
        } else {
            // Insert new character
            SQLite::Statement stmt(*persistenceService_->GetDatabase(),
                "INSERT INTO characters (name, archetype_id, level) VALUES (?, ?, ?)");
            stmt.bind(1, std::string(characterName_));
            stmt.bind(2, archetypeId);
            stmt.bind(3, characterLevel_);
            stmt.exec();

            int configId = static_cast<int>(persistenceService_->GetDatabase()->getLastInsertRowid());
            currentCharacterId_ = configId;

            // Create inventory and spellbook for new character
            SQLite::Statement invStmt(*persistenceService_->GetDatabase(),
                "INSERT INTO inventories (character_id) VALUES (?)");
            invStmt.bind(1, configId);
            invStmt.exec();

            SQLite::Statement spellStmt(*persistenceService_->GetDatabase(),
                "INSERT INTO spellbooks (character_id) VALUES (?)");
            spellStmt.bind(1, configId);
            spellStmt.exec();
        }

        RefreshCharacters();
        statusMessage_ = "Character saved successfully!";
    }
    catch (const std::exception& e) {
        statusMessage_ = "Error saving character: " + std::string(e.what());
    }
}

void CharacterService::SaveArchetype() {
    if (!persistenceService_ || !persistenceService_->GetDatabase()) {
        statusMessage_ = "Error: Database not available";
        return;
    }

    if (strlen(archetypeName_) == 0) {
        statusMessage_ = "Error: Please enter an archetype name";
        return;
    }

    try {
        if (currentArchetypeId_ != -1) {
            // Update existing archetype
            SQLite::Statement stmt(*persistenceService_->GetDatabase(),
                "UPDATE archetypes SET name = ?, str_base = ?, int_base = ?, agi_base = ?, str_growth = ?, int_growth = ?, agi_growth = ? WHERE id = ?");
            stmt.bind(1, std::string(archetypeName_));
            stmt.bind(2, strBase_);
            stmt.bind(3, intBase_);
            stmt.bind(4, agiBase_);
            stmt.bind(5, strGrowth_);
            stmt.bind(6, intGrowth_);
            stmt.bind(7, agiGrowth_);
            stmt.bind(8, currentArchetypeId_);
            stmt.exec();
        } else {
            // Insert new archetype
            SQLite::Statement stmt(*persistenceService_->GetDatabase(),
                "INSERT INTO archetypes (name, str_base, int_base, agi_base, str_growth, int_growth, agi_growth) VALUES (?, ?, ?, ?, ?, ?, ?)");
            stmt.bind(1, std::string(archetypeName_));
            stmt.bind(2, strBase_);
            stmt.bind(3, intBase_);
            stmt.bind(4, agiBase_);
            stmt.bind(5, strGrowth_);
            stmt.bind(6, intGrowth_);
            stmt.bind(7, agiGrowth_);
            stmt.exec();

            currentArchetypeId_ = static_cast<int>(persistenceService_->GetDatabase()->getLastInsertRowid());
        }

        RefreshArchetypes();
        statusMessage_ = "Archetype saved successfully!";
    }
    catch (const std::exception& e) {
        statusMessage_ = "Error saving archetype: " + std::string(e.what());
    }
}

void CharacterService::SaveItem() {
    if (!persistenceService_ || !persistenceService_->GetDatabase()) {
        statusMessage_ = "Error: Database not available";
        return;
    }

    if (strlen(itemName_) == 0) {
        statusMessage_ = "Error: Please enter an item name";
        return;
    }

    try {
        if (currentItemId_ != -1) {
            // Update existing item
            SQLite::Statement stmt(*persistenceService_->GetDatabase(),
                "UPDATE items SET name = ?, type = ?, health = ?, weapon_damage = ?, spell_power = ?, armor = ? WHERE id = ?");
            stmt.bind(1, std::string(itemName_));
            stmt.bind(2, std::string(itemType_));
            stmt.bind(3, itemHealth_);
            stmt.bind(4, itemWeaponDamage_);
            stmt.bind(5, itemSpellPower_);
            stmt.bind(6, itemArmor_);
            stmt.bind(7, currentItemId_);
            stmt.exec();
        } else {
            // Insert new item
            SQLite::Statement stmt(*persistenceService_->GetDatabase(),
                "INSERT INTO items (name, type, health, weapon_damage, spell_power, armor, ward, mana, stamina) VALUES (?, ?, ?, ?, ?, ?, 0, 0, 0)");
            stmt.bind(1, std::string(itemName_));
            stmt.bind(2, std::string(itemType_));
            stmt.bind(3, itemHealth_);
            stmt.bind(4, itemWeaponDamage_);
            stmt.bind(5, itemSpellPower_);
            stmt.bind(6, itemArmor_);
            stmt.exec();

            currentItemId_ = static_cast<int>(persistenceService_->GetDatabase()->getLastInsertRowid());
        }

        RefreshItems();
        statusMessage_ = "Item saved successfully!";
    }
    catch (const std::exception& e) {
        statusMessage_ = "Error saving item: " + std::string(e.what());
    }
}

void CharacterService::SaveSpell() {
    if (!persistenceService_ || !persistenceService_->GetDatabase()) {
        statusMessage_ = "Error: Database not available";
        return;
    }

    if (strlen(spellName_) == 0) {
        statusMessage_ = "Error: Please enter a spell name";
        return;
    }

    try {
        if (currentSpellId_ != -1) {
            // Update existing spell
            SQLite::Statement stmt(*persistenceService_->GetDatabase(),
                "UPDATE spells SET name = ?, cost_type = ?, cost_amount = ? WHERE id = ?");
            stmt.bind(1, std::string(spellName_));
            stmt.bind(2, std::string(spellCostType_));
            stmt.bind(3, spellCostAmount_);
            stmt.bind(4, currentSpellId_);
            stmt.exec();
        } else {
            // Insert new spell
            SQLite::Statement stmt(*persistenceService_->GetDatabase(),
                "INSERT INTO spells (name, cost_type, cost_amount) VALUES (?, ?, ?)");
            stmt.bind(1, std::string(spellName_));
            stmt.bind(2, std::string(spellCostType_));
            stmt.bind(3, spellCostAmount_);
            stmt.exec();

            currentSpellId_ = static_cast<int>(persistenceService_->GetDatabase()->getLastInsertRowid());
        }

        RefreshSpells();
        statusMessage_ = "Spell saved successfully!";
    }
    catch (const std::exception& e) {
        statusMessage_ = "Error saving spell: " + std::string(e.what());
    }
}

void CharacterService::DeleteCharacter() {
    if (currentCharacterId_ == -1) {
        statusMessage_ = "No character selected for deletion";
        return;
    }

    if (!persistenceService_ || !persistenceService_->GetDatabase()) {
        statusMessage_ = "Error: Database not available";
        return;
    }

    try {
        SQLite::Statement stmt(*persistenceService_->GetDatabase(),
            "DELETE FROM characters WHERE id = ?");
        stmt.bind(1, currentCharacterId_);
        stmt.exec();

        // Clear current selection
        NewCharacter();
        RefreshCharacters();
        statusMessage_ = "Character deleted successfully!";
    }
    catch (const std::exception& e) {
        statusMessage_ = "Error deleting character: " + std::string(e.what());
    }
}

void CharacterService::DeleteArchetype() {
    if (currentArchetypeId_ == -1) {
        statusMessage_ = "No archetype selected for deletion";
        return;
    }

    if (!persistenceService_ || !persistenceService_->GetDatabase()) {
        statusMessage_ = "Error: Database not available";
        return;
    }

    try {
        SQLite::Statement stmt(*persistenceService_->GetDatabase(),
            "DELETE FROM archetypes WHERE id = ?");
        stmt.bind(1, currentArchetypeId_);
        stmt.exec();

        // Clear current selection
        NewArchetype();
        RefreshArchetypes();
        statusMessage_ = "Archetype deleted successfully!";
    }
    catch (const std::exception& e) {
        statusMessage_ = "Error deleting archetype: " + std::string(e.what());
    }
}

void CharacterService::DeleteItem() {
    if (currentItemId_ == -1) {
        statusMessage_ = "No item selected for deletion";
        return;
    }

    if (!persistenceService_ || !persistenceService_->GetDatabase()) {
        statusMessage_ = "Error: Database not available";
        return;
    }

    try {
        SQLite::Statement stmt(*persistenceService_->GetDatabase(),
            "DELETE FROM items WHERE id = ?");
        stmt.bind(1, currentItemId_);
        stmt.exec();

        // Clear current selection
        NewItem();
        RefreshItems();
        statusMessage_ = "Item deleted successfully!";
    }
    catch (const std::exception& e) {
        statusMessage_ = "Error deleting item: " + std::string(e.what());
    }
}

void CharacterService::DeleteSpell() {
    if (currentSpellId_ == -1) {
        statusMessage_ = "No spell selected for deletion";
        return;
    }

    if (!persistenceService_ || !persistenceService_->GetDatabase()) {
        statusMessage_ = "Error: Database not available";
        return;
    }

    try {
        SQLite::Statement stmt(*persistenceService_->GetDatabase(),
            "DELETE FROM spells WHERE id = ?");
        stmt.bind(1, currentSpellId_);
        stmt.exec();

        // Clear current selection
        NewSpell();
        RefreshSpells();
        statusMessage_ = "Spell deleted successfully!";
    }
    catch (const std::exception& e) {
        statusMessage_ = "Error deleting spell: " + std::string(e.what());
    }
}

void CharacterService::NewCharacter() {
    memset(characterName_, 0, sizeof(characterName_));
    characterLevel_ = 10;
    currentCharacterId_ = -1;
    selectedArchetypeIndex_ = 0;
    UpdateCharacterPreview();
    RefreshCurrentInventory();
    RefreshCurrentSpellbook();
}

void CharacterService::NewArchetype() {
    memset(archetypeName_, 0, sizeof(archetypeName_));
    strBase_ = 10;
    intBase_ = 10;
    agiBase_ = 10;
    strGrowth_ = 2.0f;
    intGrowth_ = 2.0f;
    agiGrowth_ = 2.0f;
    currentArchetypeId_ = -1;
}

void CharacterService::NewItem() {
    memset(itemName_, 0, sizeof(itemName_));
    strcpy(itemType_, "weapon");
    itemHealth_ = 0;
    itemWeaponDamage_ = 0;
    itemSpellPower_ = 0;
    itemArmor_ = 0;
    currentItemId_ = -1;
}

void CharacterService::NewSpell() {
    memset(spellName_, 0, sizeof(spellName_));
    strcpy(spellCostType_, "mana");
    spellCostAmount_ = 10;
    currentSpellId_ = -1;
}

void CharacterService::AddItemToInventory() {
    if (currentCharacterId_ == -1) {
        statusMessage_ = "Please select a character first";
        return;
    }

    if (selectedItemIndex_ >= items_.size()) {
        statusMessage_ = "Please select an item to add";
        return;
    }

    if (!persistenceService_ || !persistenceService_->GetDatabase()) {
        statusMessage_ = "Error: Database not available";
        return;
    }

    try {
        int itemId = items_[selectedItemIndex_].id;

        // Get inventory ID
        SQLite::Statement getInv(*persistenceService_->GetDatabase(),
            "SELECT id FROM inventories WHERE character_id = ?");
        getInv.bind(1, currentCharacterId_);

        if (getInv.executeStep()) {
            int inventoryId = getInv.getColumn(0).getInt();

            SQLite::Statement addItem(*persistenceService_->GetDatabase(),
                "INSERT INTO inventory_items (inventory_id, item_id) VALUES (?, ?)");
            addItem.bind(1, inventoryId);
            addItem.bind(2, itemId);
            addItem.exec();

            RefreshCurrentInventory();
            statusMessage_ = "Item added to inventory!";
        } else {
            statusMessage_ = "Error: Inventory not found";
        }
    }
    catch (const std::exception& e) {
        statusMessage_ = "Error adding item: " + std::string(e.what());
    }
}

void CharacterService::AddSpellToSpellbook() {
    if (currentCharacterId_ == -1) {
        statusMessage_ = "Please select a character first";
        return;
    }

    if (selectedSpellIndex_ >= spells_.size()) {
        statusMessage_ = "Please select a spell to add";
        return;
    }

    if (!persistenceService_ || !persistenceService_->GetDatabase()) {
        statusMessage_ = "Error: Database not available";
        return;
    }

    try {
        int spellId = spells_[selectedSpellIndex_].id;

        // Get spellbook ID
        SQLite::Statement getSpellbook(*persistenceService_->GetDatabase(),
            "SELECT id FROM spellbooks WHERE character_id = ?");
        getSpellbook.bind(1, currentCharacterId_);

        if (getSpellbook.executeStep()) {
            int spellbookId = getSpellbook.getColumn(0).getInt();

            // Check if spell already exists
            SQLite::Statement checkSpell(*persistenceService_->GetDatabase(),
                "SELECT COUNT(*) FROM spellbook_spells WHERE spellbook_id = ? AND spell_id = ?");
            checkSpell.bind(1, spellbookId);
            checkSpell.bind(2, spellId);
            checkSpell.executeStep();

            if (checkSpell.getColumn(0).getInt() > 0) {
                statusMessage_ = "Spell already in spellbook!";
                return;
            }

            SQLite::Statement addSpell(*persistenceService_->GetDatabase(),
                "INSERT INTO spellbook_spells (spellbook_id, spell_id) VALUES (?, ?)");
            addSpell.bind(1, spellbookId);
            addSpell.bind(2, spellId);
            addSpell.exec();

            RefreshCurrentSpellbook();
            statusMessage_ = "Spell added to spellbook!";
        } else {
            statusMessage_ = "Error: Spellbook not found";
        }
    }
    catch (const std::exception& e) {
        statusMessage_ = "Error adding spell: " + std::string(e.what());
    }
}

void CharacterService::UpdateCharacterPreview() {
    // This is called when archetype or level changes to update preview stats
}

void CharacterService::CalculateStats(const std::string& archetypeName, int level, float& str, float& intel, float& agi, int& health, int& mana) {
    auto it = std::find_if(archetypes_.begin(), archetypes_.end(),
                          [&archetypeName](const ArchetypeData& arch) { return arch.name == archetypeName; });

    if (it != archetypes_.end()) {
        str = it->str_base + (level * it->str_growth);
        intel = it->int_base + (level * it->int_growth);
        agi = it->agi_base + (level * it->agi_growth);
        health = static_cast<int>(str * 10 + level * 5);
        mana = static_cast<int>(intel * 8 + level * 3);
    } else {
        str = intel = agi = 0;
        health = mana = 0;
    }
}

void CharacterService::ExportToJson() {
    statusMessage_ = "Export to JSON functionality not yet implemented";
}

} // namespace Elysium::Services
