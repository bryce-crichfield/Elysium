#pragma once

#include <functional>
#include <utility>
#include "Core/Future.h"

namespace Elysium::Services {

// Submit<T> can't be virtual (it's a template), so the interface exposes a
// type-erased primitive and keeps Submit<T> inline here, calling down to it.
class ITaskService {
   public:
    virtual ~ITaskService() = default;

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
