#pragma once

#include "Service.h"
#include "raylib.h"
#include <string>
#include <vector>
#include <thread>
#include <functional>
#include <atomic>
#include <mutex>
#include "Asset.h"
#include "Services/TaskService.h"

namespace Elysium::Services {

class AssetService;

class LoadTask : public Task {
public:
    LoadTask(Asset asset);
    void Execute() override;    
    std::string GetDescription() const override;
    const Asset& GetAsset() const;

private:
    Asset asset_;
};

class LoadingService : public TaskService<LoadTask> {
public:
    LoadingService() {
        name_ = "LoadingService";
    }
    
    void Initialize() override {
        TaskService<LoadTask>::Initialize();
    }
    
    void LoadAssets(const std::vector<Asset>& assets) {
        std::vector<std::unique_ptr<LoadTask>> tasks;
        tasks.reserve(assets.size());
        
        for (const auto& asset : assets) {
            tasks.push_back(std::make_unique<LoadTask>(asset));
        }
        
        for (auto& task : tasks) {
            SubmitTask(std::move(task));
        }
    }
};

} // namespace Elysium::Services
