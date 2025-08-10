#pragma once

#include <tinyxml2.h>
#include "System.h"
#include "raylib.h"
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include "Animation.h"

namespace Elysium::Xml {

class ActionParser
{
public:
    void SetParameter(const std::string& key, const std::string& value);
    void RegisterCallback(const std::string& name, std::function<void()> callback);
    std::shared_ptr<Action> LoadFromXML(const std::string& filepath);

private:
    std::string ResolveTemplate(const std::string& input);
    float GetFloatAttribute(tinyxml2::XMLElement* element, const char* name, float defaultValue = 0.0f);
    int GetIntAttribute(tinyxml2::XMLElement* element, const char* name, int defaultValue = 0);
    std::string GetStringAttribute(tinyxml2::XMLElement* element, const char* name, const std::string& defaultValue = "");
    std::shared_ptr<Action> ParseAction(tinyxml2::XMLElement* element);

    std::unordered_map<std::string, std::string> templateParameters_;
    std::unordered_map<std::string, std::function<void()>> callbacks_;
};

};
