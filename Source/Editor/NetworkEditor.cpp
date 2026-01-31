#include "NetworkEditor.h"
#include "Core/Application.h"
#include "Core/Common.h"
#include "Services/InvokeService.h"
#include "Services/LogService.h"
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
                NetworkConfig config;
                config.mode = NetworkMode::Server;
                config.port = static_cast<uint16_t>(port_);
                service.Start(config);
            }
            ImGui::SameLine();
            if (ImGui::Button("Start Client")) {
                NetworkConfig config;
                config.mode = NetworkMode::Client;
                config.address = addressBuffer_;
                config.port = static_cast<uint16_t>(port_);
                service.Start(config);
            }
        } else {
            if (ImGui::Button("Stop")) {
                service.Stop();
            }

            ImGui::Separator();
            ImGui::Text("Test RPC");
            if (ImGui::Button("Send Ping")) {
                auto& invoke = app.GetService<InvokeService>();
                PingRequest req;
                req.clientTick = ++pingCounter_;
                auto future = invoke.Invoke<Ping>(SERVER_PEER, req);
                pingFuture_ = future;
                waitingForPing_ = true;
                LOG_INFO("NetworkEditor", "Sent Ping RPC (clientTick=42)");
            }
            if (waitingForPing_ && pingFuture_.IsReady()) {
                auto resp = pingFuture_.Get();
                LOG_INFOF("NetworkEditor", "Ping response: serverTick=%u, echoClientTick=%u",
                          resp.serverTick, resp.echoClientTick);
                lastPingResponse_ = resp;
                waitingForPing_ = false;
                hasPingResult_ = true;
            }
            if (hasPingResult_) {
                ImGui::Text("Last Ping: serverTick=%u echo=%u",
                            lastPingResponse_.serverTick, lastPingResponse_.echoClientTick);
            }
        }
    }
    ImGui::End();
}

}  // namespace Elysium
