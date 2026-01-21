#include "NetworkEditor.h"
#include "Core/Application.h"
#include "Core/Common.h"
#include "Services/NetworkService.h"
#include "imgui.h"

namespace Elysium {

using namespace Services;

NetworkEditor::NetworkEditor() : Editor("Network") {}

void NetworkEditor::Draw(Application& app) {
    Profile;

    auto& service = app.GetService<NetworkService>();

    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(name_.c_str(), &isVisible_, ImGuiWindowFlags_NoCollapse)) {
        // Status
        const char* modeStr = "None";
        if (service.GetMode() == NetworkMode::Server) {
            modeStr = "Server";
        } else if (service.GetMode() == NetworkMode::Client) {
            modeStr = "Client";
        }

        ImGui::Text("Mode: %s", modeStr);
        ImGui::Text("Running: %s", service.IsRunning() ? "Yes" : "No");
        ImGui::Text("Connected Peers: %u", service.GetConnectedPeers());

        ImGui::Separator();

        // Stats
        ImGui::Text("Packets Sent: %u", service.GetPacketsSent());
        ImGui::Text("Packets Received: %u", service.GetPacketsReceived());
        ImGui::Text("Bytes Sent: %llu", service.GetBytesSent());
        ImGui::Text("Bytes Received: %llu", service.GetBytesReceived());

        ImGui::Separator();

        // Controls
        if (!service.IsRunning()) {
            ImGui::InputText("Address", addressBuffer_, sizeof(addressBuffer_));
            ImGui::InputInt("Port", &port_);

            if (ImGui::Button("Start Server")) {
                service.StartServer(port_);
            }
            ImGui::SameLine();
            if (ImGui::Button("Start Client")) {
                service.StartClient(addressBuffer_, port_);
            }
        } else {
            if (ImGui::Button("Stop")) {
                service.Stop();
            }
        }
    }
    ImGui::End();
}

}  // namespace Elysium
