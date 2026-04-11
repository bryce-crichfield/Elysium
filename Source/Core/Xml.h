#pragma once

#include <tinyxml2.h>
#include <any>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <variant>
#include <vector>
#include "Core/System.h"
#include "raylib.h"

using namespace tinyxml2;

namespace Elysium {

class XMLBuilder;  // forward declaration for XmlSavable concept

// Concept for components that can be loaded from XML
template<typename T>
concept XmlLoadable = requires(T& c, XMLElement* el) {
    { T::LoadXml(c, el) } -> std::same_as<void>;
};

// Concept for components that can be saved to XML
template<typename T>
concept XmlSavable = requires(const T& c, XMLBuilder& builder) {
    { T::SaveXml(c, builder) } -> std::same_as<void>;
};

std::string ColorToHex(Color color);

Color ParseHexColor(const std::string& hex, Color defaultColor = BLANK);

// Processes XML '<Include src="path" />' tags by loading and merging referenced files into main document
bool ProcessIncludes(tinyxml2::XMLDocument& doc, const std::string& basePath);

template <typename Func>
void VisitElement(tinyxml2::XMLElement* parent, const char* xmlName, Func func) {
    if (tinyxml2::XMLElement* element = parent->FirstChildElement(xmlName)) {
        func(element);
    }
}

// Iterates through child elements with a specific tag name
template <typename Func>
void ForEachElement(tinyxml2::XMLElement* parent, const char* xmlName, Func func) {
    for (tinyxml2::XMLElement* element = parent->FirstChildElement(xmlName);
         element != nullptr;
         element = element->NextSiblingElement(xmlName)) {
        func(element);
    }
}

// Iterates through ALL child elements (regardless of tag name)
template <typename Func>
void ForEachChild(tinyxml2::XMLElement* parent, Func func) {
    for (tinyxml2::XMLElement* child = parent->FirstChildElement();
         child != nullptr;
         child = child->NextSiblingElement()) {
        func(child);
    }
}

bool LoadXml(const std::string& filePath, tinyxml2::XMLDocument& doc);

bool SaveXml(const std::string& filePath, tinyxml2::XMLDocument& doc);

class XMLBuilder {
   private:
    tinyxml2::XMLDocument* doc;
    tinyxml2::XMLElement* current;

   public:
    XMLBuilder(tinyxml2::XMLDocument* document, tinyxml2::XMLElement* parent) : doc(document), current(parent) {}

    XMLBuilder AddElement(const char* name) {
        tinyxml2::XMLElement* elem = doc->NewElement(name);
        current->InsertEndChild(elem);
        return XMLBuilder(doc, elem);
    }

    XMLBuilder& SetAttribute(const char* name, const auto& value) {
        current->SetAttribute(name, value);
        return *this;
    }

    XMLBuilder& SetText(const auto& text) {
        current->SetText(text);
        return *this;
    }

    XMLBuilder Parent() {
        return XMLBuilder(doc, current->Parent()->ToElement());
    }
};

};  // namespace Elysium
