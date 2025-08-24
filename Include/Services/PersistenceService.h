#pragma once

#include "Service.h"
#include <string>
#include <memory>
#include <SQLiteCpp/SQLiteCpp.h>

namespace Elysium::Services {

class CharacterService;

class PersistenceService : public Elysium::Service {
public:
    PersistenceService();
    ~PersistenceService();

    // Service interface
    void Initialize() override;
    void Initialize(const std::string& databasePath = "./Assets/save.db");
    void Shutdown() override;
    void Update(float deltaTime) override;
    void OnDebugDraw() override;

    // Service-specific functionality
    bool IsInitialized() const { return initialized_; }
    SQLite::Database* GetDatabase() { return database_.get(); }

private:
    bool initialized_;
    std::unique_ptr<SQLite::Database> database_;
    std::string databasePath_;
    std::unique_ptr<CharacterService> characterService_;

    void CreateDefaultTables();
};

} // namespace Elysium::Services
