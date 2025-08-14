#pragma once

#include <string>
#include <memory>
#include <SQLiteCpp/SQLiteCpp.h>

namespace Elysium::Services {

class CharacterService;

class PersistenceService {
public:
    PersistenceService();
    ~PersistenceService();

    void Initialize(const std::string& databasePath = "./Assets/save.db");
    void Shutdown();

    void Update(float deltaTime);
    void Draw();

    bool IsInitialized() const { return initialized_; }
    SQLite::Database* GetDatabase() { return database_.get(); }

    void SetVisible(bool visible) { isVisible_ = visible; }
    bool IsVisible() const { return isVisible_; }
    void ToggleVisibility() { isVisible_ = !isVisible_; }

private:
    bool initialized_;
    bool isVisible_;
    std::unique_ptr<SQLite::Database> database_;
    std::string databasePath_;
    std::unique_ptr<CharacterService> characterService_;

    void CreateDefaultTables();
};

} // namespace Elysium::Services
