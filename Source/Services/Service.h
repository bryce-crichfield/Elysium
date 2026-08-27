#pragma once

#include <string>
#include "Core/ServiceLocator.h"

namespace Elysium {

// Lifecycle contract the ServiceLocator drives internally (Initialize/Update/
// Shutdown loop). Nothing outside Services/ or Application should ever see
// this type — consumers depend on the IXService interfaces instead.
class Service {
   public:
    Service(ServiceLocator& locator) : registry_(locator) {}
    virtual ~Service() = default;

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(float deltaTime) = 0;
    virtual void Render() {}

    std::string GetName() const { return name_; }

   protected:
    std::string name_ = "UndefinedService";
    ServiceLocator& registry_;
};

}  // namespace Elysium
