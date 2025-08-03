#include "Scene.h"
#include "GameConfig.h"

namespace Elysium {

Scene::Scene(const std::string& name, const GameConfig& config) : name_(name), config_(config) {
}

} // namespace Elysium