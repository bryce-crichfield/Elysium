#include "Services/LoadingService.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "Path.h"
#include "raylib.h"
#include "tinyxml2.h"
#include "imgui.h"
#include <chrono>
#include <sstream>
#include <regex>
#include <cmath>
#include "Common.h"
#include <tracy/Tracy.hpp>
#include "Application.h"

using namespace tinyxml2;

namespace Elysium::Services {
LoadTask::LoadTask(Asset asset) : asset_(std::move(asset)) {}

void LoadTask::Execute() {
    ProfileN("Load Asset");
    auto &assetService = Application::GetInstance().GetService<AssetService>();
    assetService.LoadAsset(asset_);
}

std::string LoadTask::GetDescription() const {
    return "LoadTask(asset = " + asset_.GetName() + ")";
}

const Asset& LoadTask::GetAsset() const {
    return asset_;
}
} // namespace Elysium::Services
