#pragma once

#include <functional>
#include <utility>
#include "Core/Future.h"
#include "Interfaces/IService.h"

namespace Elysium::Services {

class ITaskService : public IService {
   public:
    virtual void SubmitRaw(std::function<void()> task, std::function<bool()> pollCompleted) = 0;
    virtual bool IsIdle() const = 0;

    template <typename T>
    Future<T> Submit(std::function<T()> work) {
        Future<T> future;
        auto task = [future, work = std::move(work)]() mutable {
            future.Resolve(work());
        };
        auto poll = [future]() mutable -> bool { return future.Poll(); };
        SubmitRaw(std::move(task), std::move(poll));
        return future;
    }
};

}  // namespace Elysium::Services
