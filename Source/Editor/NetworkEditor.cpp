#include "NetworkEditor.h"
#include "Core/Common.h"
#include "Editor/Widgets.h"
#include "Interfaces/IInvokeService.h"
#include "Interfaces/INetworkService.h"
#include "Core/Log.h"

namespace Elysium {

using namespace Services;
using namespace Generated;

NetworkEditor::NetworkEditor(ServiceLocator& services) : Editor(services, Title) {}

void NetworkEditor::Draw() {
    Profile;

    auto& service = services_.Get<INetworkService>();

    if (!ImGui::IsPopupOpen(Title)) ImGui::OpenPopup(Title);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(Theme().DialogWidth, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(Title, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        DrawStatus(service);
        if (service.IsRunning()) {
            DrawStats(service);
            DrawPing();
        } else {
            DrawConnect(service);
        }

        ImGui::Spacing();
        ImGui::Separator();
        AlignRight(ButtonWidth("Close"));
        if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            isVisible_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void NetworkEditor::DrawStatus(INetworkService& service) {
    const char* modeStr = "Offline";
    if (service.GetMode() == NetworkMode::Server) modeStr = "Server";
    else if (service.GetMode() == NetworkMode::Client) modeStr = "Client";

    ImGui::AlignTextToFramePadding();
    ColoredText(service.IsRunning() ? Palette().Success : Palette().TextDisabled, ICON_FA_CIRCLE);
    ImGui::SameLine();
    ImGui::TextUnformatted(modeStr);
    ImGui::SameLine();
    ImGui::TextDisabled(service.IsRunning() ? "running" : "stopped");

    if (service.IsRunning()) {
        AlignRight(ButtonWidth(ICON_FA_XMARK "  Stop"));
        if (ImGui::Button(ICON_FA_XMARK "  Stop")) service.Stop();
    }
}

void NetworkEditor::DrawConnect(INetworkService& service) {
    SectionHeader("Connection");
    PropertyLabel("Address");
    ImGui::InputText("##Address", addressBuffer_, sizeof(addressBuffer_));
    PropertyLabel("Port");
    ImGui::InputInt("##Port", &port_);

    ImGui::Spacing();
    const float half = SharedButtonWidth(2);
    NetworkConfig config;
    config.port = static_cast<uint16_t>(port_);
    if (PrimaryButton(ICON_FA_SERVER "  Start Server", ImVec2(half, 0))) {
        config.mode = NetworkMode::Server;
        service.Start(config);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLUG "  Connect", ImVec2(half, 0))) {
        config.mode = NetworkMode::Client;
        config.address = addressBuffer_;
        service.Start(config);
    }
}

void NetworkEditor::DrawStats(INetworkService& service) {
    SectionHeader("Traffic");
    auto row = [](const char* label, uint64_t value) { ReadOnlyRow(label, std::to_string(value).c_str()); };
    row("Peers", service.GetConnectedPeers());
    row("Packets sent", service.GetPacketsSent());
    row("Packets received", service.GetPacketsReceived());
    row("Bytes sent", service.GetBytesSent());
    row("Bytes received", service.GetBytesReceived());
}

void NetworkEditor::DrawPing() {
    SectionHeader("Test RPC");
    if (ImGui::Button("Send Ping")) {
        PingRequest req;
        req.clientTick = ++pingCounter_;
        pingFuture_ = services_.Get<IInvokeService>().Invoke<Ping>(SERVER_PEER, req);
        waitingForPing_ = true;
        LOG_INFOF("NetworkEditor", "Sent Ping RPC (clientTick=%u)", req.clientTick);
    }
    if (waitingForPing_ && pingFuture_.IsReady()) {
        lastPingResponse_ = pingFuture_.Get();
        LOG_INFOF("NetworkEditor", "Ping response: serverTick=%u, echoClientTick=%u",
                  lastPingResponse_.serverTick, lastPingResponse_.echoClientTick);
        waitingForPing_ = false;
        hasPingResult_ = true;
    }
    ImGui::SameLine();
    if (waitingForPing_) {
        ImGui::TextDisabled("Waiting...");
    } else if (hasPingResult_) {
        ImGui::TextDisabled("server tick %u, echo %u", lastPingResponse_.serverTick, lastPingResponse_.echoClientTick);
    }
}

}  // namespace Elysium
