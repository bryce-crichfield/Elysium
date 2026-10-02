#include "Core/Components/ParentComponent.h"
#include "Core/ComponentRegistry.h"

namespace Elysium {

    void ParentComponent::LoadXml(ParentComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        const char* target = el->Attribute("target");
        c.targetName = target ? target : "";
        // parent (Entity ID) is populated later by the hierarchy resolution pass
    }

    void ParentComponent::SaveXml(const ParentComponent& c, XMLBuilder& builder) {
        if (!c.targetName.empty()) {
            builder.AddElement("ParentComponent")
                .SetAttribute("target", c.targetName.c_str());
        }
    }

    void ParentComponent::BindLua(sol::usertype<ParentComponent>& ut) {
        ut["parent"]     = &ParentComponent::parent;
        ut["targetName"] = &ParentComponent::targetName;
    }

    REGISTER_COMPONENT(ParentComponent);
}
