#pragma once

#include <string>
#include <unordered_set>
#include <vector>
#include "Core/Log.h"
#include "Core/ServiceLocator.h"
#include "Interfaces/ILogService.h"

namespace Elysium::Services {

// LogService for the browser: no writer thread and no log file. Each line goes straight to
// stdout, which Emscripten forwards to the browser console.
class WebLogService : public ILogService {
   public:
    explicit WebLogService(ServiceLocator& registry);
    ~WebLogService() override;

    void Initialize() override {}
    void Shutdown() override {}
    void Update(float) override {}

    void LogMessage(int logLevel, const std::string& message) override;
    void LogMessage(LogLevel level, const std::string& topic, const std::string& message) override;

    const std::vector<LogEntry>& GetLogBuffer() const override { return logBuffer_; }
    std::vector<std::string> GetAllTopics() const override;
    std::string FormatTimestamp(const std::chrono::system_clock::time_point& timestamp) const override;
    std::string FormatLogEntry(const LogEntry& entry) const override;

   private:
    void Record(const LogEntry& entry);

    static constexpr size_t MaxLogBufferSize = 1000;

    std::vector<LogEntry> logBuffer_;
    std::unordered_set<std::string> topics_;
};

}  // namespace Elysium::Services
