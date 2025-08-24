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
#include <tinyxml2.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>
#include <any>

namespace Elysium {


class XmlVisitor {
public:
    using SerializeFunc = std::function<void(const std::any&, tinyxml2::XMLElement*, tinyxml2::XMLDocument&)>;
    using DeserializeFunc = std::function<std::any(const tinyxml2::XMLElement*)>;

private:
    std::unordered_map<std::string, SerializeFunc> serializers;
    std::unordered_map<std::string, DeserializeFunc> deserializers;
    std::unordered_map<std::type_index, std::string> typeToTag;

public:
    template<typename T>
    void OnTag(const std::string& tagName,
               std::function<void(const T&, tinyxml2::XMLElement*, tinyxml2::XMLDocument&)> serialize,
               std::function<T(const tinyxml2::XMLElement*)> deserialize) {

        // Store type -> tag mapping
        typeToTag[std::type_index(typeid(T))] = tagName;

        // Wrap serializer with type erasure
        serializers[tagName] = [serialize](const std::any& obj, tinyxml2::XMLElement* element, tinyxml2::XMLDocument& doc) {
            try {
                const T& typed_obj = std::any_cast<const T&>(obj);
                serialize(typed_obj, element, doc);
            } catch (const std::bad_any_cast& e) {
                // Handle error - wrong type for this tag
            }
        };

        // Wrap deserializer with type erasure
        deserializers[tagName] = [deserialize](const tinyxml2::XMLElement* element) -> std::any {
            return std::make_any<T>(deserialize(element));
        };
    }

    template<typename T>
    void Serialize(const T& obj, tinyxml2::XMLElement* parent, tinyxml2::XMLDocument& doc) {
        auto typeIndex = std::type_index(typeid(T));
        auto tagIt = typeToTag.find(typeIndex);
        if (tagIt == typeToTag.end()) return;

        const std::string& tagName = tagIt->second;
        auto element = doc.NewElement(tagName.c_str());

        auto serIt = serializers.find(tagName);
        if (serIt != serializers.end()) {
            serIt->second(std::make_any<T>(obj), element, doc);
            parent->InsertEndChild(element);
        }
    }

    template<typename T>
    bool Deserialize(const tinyxml2::XMLElement* element, T& obj) {
        std::string tagName = element->Name();
        auto deserIt = deserializers.find(tagName);
        if (deserIt == deserializers.end()) return false;

        try {
            std::any result = deserIt->second(element);
            obj = std::any_cast<T>(result);
            return true;
        } catch (const std::bad_any_cast& e) {
            return false;
        }
    }

    // Visit all child elements and deserialize known types
    void VisitChildren(const tinyxml2::XMLElement* parent,
                      std::function<void(const std::string&, std::any)> onElement) {
        for (const auto* child = parent->FirstChildElement(); child; child = child->NextSiblingElement()) {
            std::string tagName = child->Name();
            auto deserIt = deserializers.find(tagName);
            if (deserIt != deserializers.end()) {
                std::any obj = deserIt->second(child);
                onElement(tagName, obj);
            }
        }
    }
};

// Enhanced context that combines both approaches
struct XmlContext {
    XmlVisitor visitor;

    template<typename T>
    void RegisterTag(const std::string& tagName,
                    std::function<void(const T&, tinyxml2::XMLElement*, tinyxml2::XMLDocument&)> serialize,
                    std::function<T(const tinyxml2::XMLElement*)> deserialize) {
        visitor.OnTag<T>(tagName, serialize, deserialize);
    }

    template<typename T>
    void Serialize(const T& obj, tinyxml2::XMLElement* parent, tinyxml2::XMLDocument& doc) {
        visitor.Serialize(obj, parent, doc);
    }

    template<typename T>
    bool Deserialize(const tinyxml2::XMLElement* element, T& obj) {
        return visitor.Deserialize(element, obj);
    }

    void VisitChildren(const tinyxml2::XMLElement* parent,
                      std::function<void(const std::string&, std::any)> onElement) {
        visitor.VisitChildren(parent, onElement);
    }
};

Color ParseHexColor(const std::string& hex, Color defaultColor = BLANK);

// Processes XML '<Include src="path" />' tags by loading and merging referenced files into main document
void ProcessIncludes(tinyxml2::XMLDocument& doc, const std::string& basePath);

};
