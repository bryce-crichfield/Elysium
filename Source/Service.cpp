#include "Service.h"
#include "imgui.h"

namespace Elysium
{
std::string Service::GetName()
{
    return name_;
}

void Service::ToggleVisibility()
{
    isVisible_ = !isVisible_;
}

void Service::DebugDraw()
{
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
}
} // namespace Elysium
