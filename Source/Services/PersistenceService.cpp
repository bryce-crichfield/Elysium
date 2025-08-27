#include "Services/PersistenceService.h"
#include "Services/CharacterService.h"
#include "Services/LogService.h"
#include "Asset.h"
#include "Path.h"
#include "imgui.h"
#include <filesystem>

namespace Elysium::Services {

PersistenceService::PersistenceService()
    : initialized_(false) {
    name_ = "PersistenceService";
}

PersistenceService::~PersistenceService() {
    Shutdown();
}

void PersistenceService::Initialize() {
    Initialize("save.db");
    isVisible_ = false;
}

void PersistenceService::Initialize(const std::string& databasePath) {
    if (initialized_) return;

    databasePath_ = Path(databasePath).GetFullPath();

    try {
        std::filesystem::path dbPath(databasePath_);
        std::filesystem::create_directories(dbPath.parent_path());

        database_ = std::make_unique<SQLite::Database>(databasePath_,
            SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

        // Create default tables if they don't exist
        CreateDefaultTables();

        // Initialize CharacterService
        characterService_ = std::make_unique<CharacterService>(this);

        LOG_INFO("PersistenceService", "Database opened successfully: " + databasePath_);
        initialized_ = true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("PersistenceService", "Failed to open database: " + std::string(e.what()));
        database_.reset();
        initialized_ = false;
    }
}

void PersistenceService::Shutdown() {
    if (!initialized_) return;

    try {
        characterService_.reset();
        if (database_) {
            database_.reset();
            LOG_INFO("PersistenceService", "Database closed successfully");
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("PersistenceService", "Error closing database: " + std::string(e.what()));
    }

    initialized_ = false;
}

void PersistenceService::Update(float deltaTime) {
}

void PersistenceService::OnDebugDraw() {
    if (!initialized_ || !characterService_) return;

        ImGui::Text("Database Status: %s", initialized_ ? "Connected" : "Disconnected");
        ImGui::Text("Database Path: %s", databasePath_.c_str());

        ImGui::Separator();

        characterService_->OnDebugDraw();
}

void PersistenceService::CreateDefaultTables() {
    if (!database_) return;

    try {
        // Create archetypes table
        database_->exec(R"(
            CREATE TABLE IF NOT EXISTS archetypes (
                id INTEGER PRIMARY KEY,
                name TEXT UNIQUE,
                str_base INTEGER,
                int_base INTEGER,
                agi_base INTEGER,
                str_growth REAL,
                int_growth REAL,
                agi_growth REAL
            )
        )");

        // Create items table
        database_->exec(R"(
            CREATE TABLE IF NOT EXISTS items (
                id INTEGER PRIMARY KEY,
                name TEXT UNIQUE,
                type TEXT,
                health INTEGER DEFAULT 0,
                weapon_damage INTEGER DEFAULT 0,
                spell_power INTEGER DEFAULT 0,
                armor INTEGER DEFAULT 0,
                ward INTEGER DEFAULT 0,
                mana INTEGER DEFAULT 0,
                stamina INTEGER DEFAULT 0
            )
        )");

        // Create spells table
        database_->exec(R"(
            CREATE TABLE IF NOT EXISTS spells (
                id INTEGER PRIMARY KEY,
                name TEXT UNIQUE,
                cost_type TEXT CHECK(cost_type IN ('stamina', 'mana')),
                cost_amount INTEGER
            )
        )");

        // Create characters table
        database_->exec(R"(
            CREATE TABLE IF NOT EXISTS characters (
                id INTEGER PRIMARY KEY,
                name TEXT UNIQUE,
                archetype_id INTEGER,
                level INTEGER DEFAULT 1,
                FOREIGN KEY (archetype_id) REFERENCES archetypes(id)
            )
        )");

        // Create inventories table
        database_->exec(R"(
            CREATE TABLE IF NOT EXISTS inventories (
                id INTEGER PRIMARY KEY,
                character_id INTEGER UNIQUE,
                FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
            )
        )");

        // Create inventory_items table
        database_->exec(R"(
            CREATE TABLE IF NOT EXISTS inventory_items (
                id INTEGER PRIMARY KEY,
                inventory_id INTEGER,
                item_id INTEGER,
                FOREIGN KEY (inventory_id) REFERENCES inventories(id) ON DELETE CASCADE,
                FOREIGN KEY (item_id) REFERENCES items(id)
            )
        )");

        // Create spellbooks table
        database_->exec(R"(
            CREATE TABLE IF NOT EXISTS spellbooks (
                id INTEGER PRIMARY KEY,
                character_id INTEGER UNIQUE,
                FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
            )
        )");

        // Create spellbook_spells table
        database_->exec(R"(
            CREATE TABLE IF NOT EXISTS spellbook_spells (
                id INTEGER PRIMARY KEY,
                spellbook_id INTEGER,
                spell_id INTEGER,
                FOREIGN KEY (spellbook_id) REFERENCES spellbooks(id) ON DELETE CASCADE,
                FOREIGN KEY (spell_id) REFERENCES spells(id)
            )
        )");
    }
    catch (const std::exception& e) {
        LOG_ERROR("PersistenceService", "Error creating tables: " + std::string(e.what()));
    }
}

} // namespace Elysium::Services
