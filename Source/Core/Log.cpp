#include "Core/Log.h"
#include "raylib.h"

namespace Elysium::Log {

void Info(const std::string& topic, const std::string& message) {
    TraceLog(LOG_INFO, "[%s] - %s", topic.c_str(), message.c_str());
}

void Warning(const std::string& topic, const std::string& message) {
    TraceLog(LOG_WARNING, "[%s] - %s", topic.c_str(), message.c_str());
}

void Error(const std::string& topic, const std::string& message) {
    TraceLog(LOG_ERROR, "[%s] - %s", topic.c_str(), message.c_str());
}

void Debug(const std::string& topic, const std::string& message) {
    TraceLog(LOG_DEBUG, "[%s] - %s", topic.c_str(), message.c_str());
}

}  // namespace Elysium::Log
