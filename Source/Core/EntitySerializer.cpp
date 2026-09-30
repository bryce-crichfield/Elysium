#include "Core/EntitySerializer.h"

#include <tinyxml2.h>
#include <unordered_map>
#include <vector>
#include "Components/PrefabInstanceComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"

namespace Elysium::EntityXml {

namespace {

// ParentComponent is deliberately never serialized here. It records its parent by name, and
// SaveSubtree records the hierarchy by index instead; World::AddChild rebuilds the component
// and the adjacency together on load, so writing it too would mean two sources of truth for
// the same link.
constexpr const char* kHierarchyComponent = "Parent";

// The placement tag travels as attributes on <Entity>, not as a component element.
//
// PrefabInstanceComponent has no XML saver on purpose: a scene writes placements as
// <PrefabInstance> blocks, and a component tag would be a second, conflicting representation.
// But that left the tag falling off anything that round-tripped through here -- so undoing a
// delete, or redoing a placement, brought a prefab back as loose entities, and the next save
// wrote them out flattened. Attributes keep it out of the component namespace entirely, so no
// scene loader can ever pick them up, while a subtree still restores as the placement it was.
constexpr const char* kPrefabSrc = "prefabSrc";
constexpr const char* kPrefabOwnerDir = "prefabOwnerDir";
constexpr const char* kPrefabInstanceId = "prefabInstanceId";
constexpr const char* kPrefabLocalId = "prefabLocalId";

void SavePlacementTag(tinyxml2::XMLElement* element, World& world, Entity entity) {
    if (!world.HasComponent<PrefabInstanceComponent>(entity)) return;
    const auto& tag = world.GetComponent<PrefabInstanceComponent>(entity);
    element->SetAttribute(kPrefabSrc, tag.src.c_str());
    element->SetAttribute(kPrefabOwnerDir, tag.ownerDir.c_str());
    element->SetAttribute(kPrefabInstanceId, tag.instanceId.c_str());
    element->SetAttribute(kPrefabLocalId, tag.localEntityId);
}

void LoadPlacementTag(tinyxml2::XMLElement* element, World& world, Entity entity) {
    const char* instanceId = element->Attribute(kPrefabInstanceId);
    if (!instanceId) return;
    PrefabInstanceComponent tag;
    tag.src = element->Attribute(kPrefabSrc) ? element->Attribute(kPrefabSrc) : "";
    tag.ownerDir = element->Attribute(kPrefabOwnerDir) ? element->Attribute(kPrefabOwnerDir) : "";
    tag.instanceId = instanceId;
    tag.localEntityId = element->IntAttribute(kPrefabLocalId, -1);
    world.AddComponent<PrefabInstanceComponent>(entity, tag);
}

std::string PrintElement(const tinyxml2::XMLElement* element) {
    tinyxml2::XMLPrinter printer;
    element->Accept(&printer);
    return printer.CStr();
}

}  // namespace

std::string SaveComponent(World& world, Entity entity, const std::string& component) {
    const auto* ops = ComponentRegistry::Instance().GetXmlOps(component);
    if (!ops) return {};

    tinyxml2::XMLDocument scratch;
    tinyxml2::XMLElement* element = ops->serialize(scratch, &world, entity);
    return element ? PrintElement(element) : std::string{};
}

void LoadComponent(World& world, Entity entity, const std::string& component,
                   const std::string& xml, ServiceLocator& services) {
    const auto* ops = ComponentRegistry::Instance().GetXmlOps(component);
    if (!ops) return;

    if (xml.empty()) {
        ops->remove(&world, entity);
        return;
    }

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) return;
    if (tinyxml2::XMLElement* element = doc.RootElement()) {
        ops->apply(element, &world, entity, services);
    }
}

std::string SaveSubtree(World& world, Entity root) {
    // Depth-first with root first, which is also the order entities must be recreated in: a
    // child's parent index always refers to an entity earlier in the list.
    const std::vector<Entity> subtree = world.GetSubtree(root);
    if (subtree.empty()) return {};

    std::unordered_map<Entity, int> indexOf;
    for (size_t i = 0; i < subtree.size(); i++) indexOf[subtree[i]] = (int)i;

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* docRoot = doc.NewElement("Subtree");
    doc.InsertFirstChild(docRoot);
    XMLBuilder builder(&doc, docRoot);

    const auto& savers = ComponentRegistry::Instance().GetXmlSavers();

    for (Entity entity : subtree) {
        auto entityBuilder = builder.AddElement("Entity");

        // A parent outside the subtree is not recorded: LoadSubtree's caller decides where the
        // restored root goes, which is how paste can drop a copy somewhere else entirely.
        const Entity parent = world.GetParent(entity);
        auto parentIndex = indexOf.find(parent);
        if (parentIndex != indexOf.end()) entityBuilder.SetAttribute("parent", parentIndex->second);

        SavePlacementTag(entityBuilder.GetElement(), world, entity);

        for (const auto& [name, saver] : savers) {
            if (name == kHierarchyComponent) continue;
            saver(entityBuilder, &world, entity);
        }
    }

    return PrintElement(docRoot);
}

Entity LoadSubtree(World& world, const std::string& xml, Entity parent, ServiceLocator& services,
                   std::vector<Entity>* outCreated) {
    if (xml.empty()) return INVALID_ENTITY;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) return INVALID_ENTITY;

    tinyxml2::XMLElement* docRoot = doc.RootElement();
    if (!docRoot) return INVALID_ENTITY;

    const auto& loaders = ComponentRegistry::Instance().GetXmlLoaders();
    std::vector<Entity> created;

    ForEachElement(docRoot, "Entity", [&](tinyxml2::XMLElement* xmlEntity) {
        const Entity entity = world.CreateEntity();
        created.push_back(entity);

        LoadPlacementTag(xmlEntity, world, entity);

        ForEachChild(xmlEntity, [&](tinyxml2::XMLElement* component) {
            auto loader = loaders.find(component->Name());
            if (loader != loaders.end()) loader->second(component, &world, entity, services);
        });
    });

    if (created.empty()) return INVALID_ENTITY;

    // Hierarchy second, once every entity exists. AddChild writes the ParentComponent and the
    // adjacency, so it has to run after the components that came out of the file.
    int index = 0;
    for (tinyxml2::XMLElement* xmlEntity = docRoot->FirstChildElement("Entity"); xmlEntity;
         xmlEntity = xmlEntity->NextSiblingElement("Entity"), index++) {
        int parentIndex = -1;
        if (xmlEntity->QueryIntAttribute("parent", &parentIndex) != tinyxml2::XML_SUCCESS) continue;
        if (parentIndex < 0 || parentIndex >= (int)created.size()) continue;
        world.AddChild(created[parentIndex], created[index]);
    }

    if (parent != INVALID_ENTITY) world.AddChild(parent, created[0]);

    if (outCreated) *outCreated = created;
    return created[0];
}

}  // namespace Elysium::EntityXml
