#include "Service.h"
#include "imgui.h"
#include "Common.h"

namespace Elysium
{
std::string Service::GetName()
{
    Profile;
    return name_;
}

void Service::ToggleVisibility()
{
    Profile;
    isVisible_ = !isVisible_;
}

void Service::DebugDraw()
{
    Profile;
    if (!isVisible_ || !hasUi_)
        return;

    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(GetName().c_str(), &isVisible_, ImGuiWindowFlags_NoCollapse))
    {

        OnDebugDraw();
    }
    ImGui::End();
}

void Service::OnDebugDraw()
{
    Profile;
}
} // namespace Elysium
