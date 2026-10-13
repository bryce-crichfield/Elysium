#include <doctest/doctest.h>
#include <algorithm>
#include <string>
#include "Core/ComponentRegistry.h"
#include "Fakes/FakeServices.h"

using namespace Elysium;

namespace {

// Two sets of values, so every bool is tried both ways and no field can pass by happening to
// equal its default.
void SetValue(const FieldInfo& field, void* component, int pass) {
    void* value = field.address(component);
    switch (field.type) {
        case FieldType::Bool:    *static_cast<bool*>(value) = pass == 0; break;
        case FieldType::Int:
        case FieldType::Choice:  *static_cast<int*>(value) = pass == 0 ? 1 : 3; break;
        case FieldType::Float:   *static_cast<float*>(value) = pass == 0 ? 0.25f : 0.75f; break;
        case FieldType::Vector2: *static_cast<Vector2*>(value) = pass == 0 ? Vector2{1, 2} : Vector2{3, 4}; break;
        case FieldType::Color:
            *static_cast<Color*>(value) = pass == 0 ? Color{10, 20, 30, 255} : Color{200, 100, 50, 128};
            break;
        case FieldType::String:
        case FieldType::Text:
        case FieldType::Asset:   *static_cast<std::string*>(value) = pass == 0 ? "a" : "b"; break;
    }
}

std::string Describe(const FieldInfo& field, void* component) {
    void* value = field.address(component);
    switch (field.type) {
        case FieldType::Bool:    return *static_cast<bool*>(value) ? "true" : "false";
        case FieldType::Int:
        case FieldType::Choice:  return std::to_string(*static_cast<int*>(value));
        case FieldType::Float:   return std::to_string(*static_cast<float*>(value));
        case FieldType::Vector2: {
            Vector2 v = *static_cast<Vector2*>(value);
            return std::to_string(v.x) + "," + std::to_string(v.y);
        }
        case FieldType::Color: {
            Color c = *static_cast<Color*>(value);
            return std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + "," + std::to_string(c.a);
        }
        case FieldType::String:
        case FieldType::Text:
        case FieldType::Asset:   return *static_cast<std::string*>(value);
    }
    return {};
}

}  // namespace

// Every reflected component that XML can round-trip: set each serialized field, save, load the
// result onto a fresh entity, and expect the same values back.
TEST_CASE("Reflected components round-trip every serialized field through XML") {
    Tests::FakeServices services;
    const ComponentRegistry& registry = ComponentRegistry::Instance();

    for (const auto& [name, type] : registry.GetComponentTypes()) {
        const std::string& tag = registry.GetXmlTag(name);
        const FieldList* fields = registry.GetFields(name);
        if (tag.empty() || !fields) continue;
        const ComponentRegistry::XmlComponentOps* ops = registry.GetXmlOps(tag);
        REQUIRE(ops);

        for (int pass = 0; pass < 2; ++pass) {
            CAPTURE(name);
            CAPTURE(pass);
            World world;
            tinyxml2::XMLDocument input;
            tinyxml2::XMLElement* empty = input.NewElement(tag.c_str());
            input.InsertFirstChild(empty);

            Entity original = world.CreateEntity();
            ops->apply(empty, &world, original, services.locator);
            for (const FieldInfo& field : *fields) {
                if (field.Serialized()) SetValue(field, ops->get(&world, original), pass);
            }

            tinyxml2::XMLDocument scratch;
            tinyxml2::XMLElement* saved = ops->serialize(scratch, &world, original);
            REQUIRE(saved);

            Entity loaded = world.CreateEntity();
            ops->apply(saved, &world, loaded, services.locator);
            for (const FieldInfo& field : *fields) {
                if (!field.Serialized()) continue;
                CAPTURE(field.key);
                CHECK(Describe(field, ops->get(&world, loaded)) == Describe(field, ops->get(&world, original)));
            }
        }
    }
}

// LoadXml must queue every asset field's file so a scene's assets load before it starts.
TEST_CASE("Loading a component requests the assets its asset fields name") {
    Tests::FakeServices services;
    const ComponentRegistry& registry = ComponentRegistry::Instance();

    for (const auto& [name, type] : registry.GetComponentTypes()) {
        const std::string& tag = registry.GetXmlTag(name);
        const FieldList* fields = registry.GetFields(name);
        if (tag.empty() || !fields) continue;
        for (const FieldInfo& field : *fields) {
            if (field.type != FieldType::Asset || !field.Serialized()) continue;
            // Known gap: RenderContext loads fonts itself, synchronously on first draw, instead of
            // through IAssetService. Remove this once fonts go through the asset pipeline.
            if (name == "Text" && field.key == "font") continue;
            CAPTURE(name);
            CAPTURE(field.key);
            World world;
            tinyxml2::XMLDocument input;
            tinyxml2::XMLElement* element = input.NewElement(tag.c_str());
            element->SetAttribute(field.key.c_str(), "Some/Asset.file");
            input.InsertFirstChild(element);

            services.assets.requested.clear();
            registry.GetXmlOps(tag)->apply(element, &world, world.CreateEntity(), services.locator);
            CHECK(std::find(services.assets.requested.begin(), services.assets.requested.end(), "Some/Asset.file") !=
                  services.assets.requested.end());
        }
    }
}
