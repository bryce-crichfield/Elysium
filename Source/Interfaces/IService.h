#pragma once

namespace Elysium::Services {

class IService {
   public:
    virtual ~IService() = default;

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(float deltaTime) = 0;
    virtual void Render() {}
};

}  // namespace Elysium::Services
