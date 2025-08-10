#pragma once

#include "Xml.h"
#include <stdexcept>
#include <tinyxml2.h>

namespace Elysium::Xml
{

void ActionParser::SetParameter(const std::string &key, const std::string &value)
{
    templateParameters_[key] = value;
}

void ActionParser::RegisterCallback(const std::string &name, std::function<void()> callback)
{
    callbacks_[name] = callback;
}

std::shared_ptr<Action> ActionParser::LoadFromXML(const std::string &filepath)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filepath.c_str()) != tinyxml2::XML_SUCCESS)
    {
        throw std::runtime_error("Failed to load XML file: " + filepath);
    }

    return ParseAction(doc.RootElement());
}

std::string ActionParser::ResolveTemplate(const std::string &input)
{
    std::string result = input;
    size_t pos = 0;

    while ((pos = result.find('{', pos)) != std::string::npos)
    {
        size_t end = result.find('}', pos);
        if (end == std::string::npos)
        {
            throw std::runtime_error("Unclosed template parameter: " + input);
        }

        std::string paramName = result.substr(pos + 1, end - pos - 1);
        auto it = templateParameters_.find(paramName);
        if (it == templateParameters_.end())
        {
            throw std::runtime_error("Template parameter not found: " + paramName);
        }

        result.replace(pos, end - pos + 1, it->second);
        pos += it->second.length();
    }

    return result;
}

float ActionParser::GetFloatAttribute(tinyxml2::XMLElement *element, const char *name, float defaultValue)
{
    const char *attr = element->Attribute(name);
    if (!attr)
        return defaultValue;

    std::string resolved = ResolveTemplate(attr);
    return std::stof(resolved);
}

int ActionParser::GetIntAttribute(tinyxml2::XMLElement *element, const char *name, int defaultValue)
{
    const char *attr = element->Attribute(name);
    if (!attr)
        return defaultValue;

    std::string resolved = ResolveTemplate(attr);
    return std::stoi(resolved);
}

std::string ActionParser::GetStringAttribute(tinyxml2::XMLElement *element, const char *name,
                                             const std::string &defaultValue)
{
    const char *attr = element->Attribute(name);
    if (!attr)
        return defaultValue;

    return ResolveTemplate(attr);
}

std::shared_ptr<Action> ActionParser::ParseAction(tinyxml2::XMLElement *element)
{
    throw std::runtime_error("Unimplemented exception");
}
} // namespace Elysium::Xml
