#include "Services/LoadingService.h"
#include <chrono>
#include <cmath>
#include <regex>
#include <sstream>
#include <tracy/Tracy.hpp>
#include "Application.h"
#include "Common.h"
#include "Path.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "imgui.h"
#include "raylib.h"
#include "tinyxml2.h"

using namespace tinyxml2;

namespace Elysium::Services {
LoadTask::LoadTask(Asset asset) : asset_(std::move(asset)) {}

void LoadTask::Execute() {
    ProfileN("Load Asset");

    // TODO: Why call into someone do LoadAsset instead of LoadTask owning this load logic?
    auto& assetService = Application::GetInstance().GetService<AssetService>();
    assetService.LoadAsset(asset_);
}

std::string LoadTask::GetDescription() const {
    return "LoadTask(asset = " + asset_.GetName() + ")";
}

const Asset& LoadTask::GetAsset() const {
    return asset_;
}
}  // namespace Elysium::Services
