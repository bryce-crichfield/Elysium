#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/ServiceLocator.h"
#include "Interfaces/IApplicationService.h"
#include "Interfaces/IAssetService.h"

namespace Elysium::Tests {

// Records load requests and never loads anything: components queue preloads in LoadXml, and
// a test only cares that they asked.
class FakeAssetService : public Services::IAssetService {
   public:
    void Initialize() override {}
    void Shutdown() override {}
    void Update(float) override {}

    Future<IAsset*> ReloadAsset(IAsset*) override { return {}; }
    void FinalizeAssets() override {}
    bool IsAssetLoaded(Path) const override { return false; }
    bool IsLoading() const override { return false; }
    IAsset* GetAsset(Path) override { return nullptr; }
    const std::unordered_map<Path, std::unique_ptr<IAsset>>& GetAllAssets() const override { return assets_; }

    std::vector<std::string> requested;

   protected:
    Future<IAsset*> LoadAssetRaw(Path path, std::function<std::unique_ptr<IAsset>(Path)>) override {
        requested.push_back(path.GetRelativePath());
        return {};
    }
    IAsset* LoadAssetNowRaw(Path path, std::function<std::unique_ptr<IAsset>(Path)>, bool) override {
        requested.push_back(path.GetRelativePath());
        return nullptr;
    }

   private:
    std::unordered_map<Path, std::unique_ptr<IAsset>> assets_;
};

class FakeApplicationService : public Services::IApplicationService {
   public:
    void Initialize() override {}
    void Shutdown() override {}
    void Update(float) override {}

    const ApplicationConfig& GetConfig() const override { return config; }
    AppMode GetMode() const override { return AppMode::Play; }
    void SetMode(AppMode) override {}
    float GetTime() const override { return 0.0f; }
    bool ShouldClose() const override { return false; }
    void RequestClose() override {}
    void RequestOpenProject(const std::string&) override {}
    int GetWindowWidth() const override { return config.windowWidth; }
    int GetWindowHeight() const override { return config.windowHeight; }

    ApplicationConfig config;
};

// A ServiceLocator with the fakes Core code reaches for while loading.
struct FakeServices {
    ServiceLocator locator;
    FakeAssetService& assets;
    FakeApplicationService& application;

    FakeServices()
        : assets(locator.Register<FakeAssetService, Services::IAssetService>(std::make_unique<FakeAssetService>())),
          application(locator.Register<FakeApplicationService, Services::IApplicationService>(
              std::make_unique<FakeApplicationService>())) {}
};

}  // namespace Elysium::Tests
