#include <doctest/doctest.h>
#include <cstring>
#include <memory>
#include <optional>
#include "Core/Generated.h"
#include "Core/Network.h"
#include "Fakes/FakeNetworkService.h"
#include "Services/InvokeService.h"
#include "Services/MessageService.h"

using namespace Elysium;
using namespace Elysium::Generated;

namespace {

constexpr NetworkPeer Peer = 7;
constexpr uint16_t PingId = Services::InvokeMethod<Ping>::Id;
constexpr uint16_t SpawnPlayerId = Services::InvokeMethod<SpawnPlayer>::Id;

struct Reply {
    PacketHeader packet;
    InvokeHeader invoke;
    SerialBuffer payload;
};

// An InvokeService wired to a real MessageService and a fake network: packets go in as
// NetworkDataMessages, the way NetworkService delivers them, and replies come out of the fake.
struct InvokeFixture {
    ServiceLocator locator;
    Services::MessageService& messages;
    Tests::FakeNetworkService& network;
    Services::InvokeService& invoke;

    InvokeFixture()
        : messages(locator.Register<Services::MessageService, Services::IMessageService>(
              std::make_unique<Services::MessageService>(locator))),
          network(locator.Register<Tests::FakeNetworkService, Services::INetworkService>(
              std::make_unique<Tests::FakeNetworkService>())),
          invoke(locator.Register<Services::InvokeService, Services::IInvokeService>(
              std::make_unique<Services::InvokeService>(locator))) {
        invoke.Initialize();
        invoke.Register<Ping>(std::function<PingResponse(NetworkPeer, const PingRequest&)>(
            [](NetworkPeer, const PingRequest& request) {
                PingResponse response;
                response.echoClientTick = request.clientTick;
                return response;
            }));
        invoke.Register<SpawnPlayer>(std::function<SpawnPlayerResponse(NetworkPeer, const SpawnPlayerRequest&)>(
            [](NetworkPeer, const SpawnPlayerRequest&) {
                SpawnPlayerResponse response;
                response.success = true;
                return response;
            }));
    }

    void Receive(const SerialBuffer& packet, NetworkChannel channel = NetworkChannel::Reliable) {
        Receive(std::vector<uint8_t>(packet.Data(), packet.Data() + packet.Size()), channel);
    }
    void Receive(std::vector<uint8_t> bytes, NetworkChannel channel = NetworkChannel::Reliable) {
        messages.Post<NetworkDataMessage>(Peer, std::move(bytes), static_cast<uint8_t>(channel));
        messages.Update(0.0f);
    }

    // The one packet sent since the last call, or nothing.
    std::optional<Reply> TakeReply() {
        if (network.sent.empty()) return std::nullopt;
        REQUIRE(network.sent.size() == 1);
        Reply reply{{}, {}, SerialBuffer(network.sent.front())};
        network.sent.clear();
        PacketHeader::Read(reply.packet, reply.payload);
        InvokeHeader::Read(reply.invoke, reply.payload);
        return reply;
    }
};

template <typename Request>
SerialBuffer InvokeRequest(uint32_t invokeId, uint16_t procedureId, const Request& request) {
    return PacketWriter().BeginPacket(PacketType::InvokeRequest).WriteInvokeHeader(invokeId, procedureId).Write(request).Build();
}

SerialBuffer InvokeRequestBytes(uint32_t invokeId, uint16_t procedureId, const void* bytes, size_t size) {
    return PacketWriter().BeginPacket(PacketType::InvokeRequest).WriteInvokeHeader(invokeId, procedureId).WriteBytes(bytes, size).Build();
}

}  // namespace

TEST_CASE("An invoke request gets a response carrying the handler's result") {
    InvokeFixture fixture;
    PingRequest request;
    request.clientTick = 12345;
    fixture.Receive(InvokeRequest(1, PingId, request));

    std::optional<Reply> reply = fixture.TakeReply();
    REQUIRE(reply);
    CHECK(reply->packet.packetType == static_cast<uint8_t>(PacketType::InvokeResponse));
    CHECK(reply->invoke.invokeId == 1);
    CHECK(reply->invoke.procedureId == PingId);
    PingResponse response;
    PingResponse::Read(response, reply->payload);
    CHECK(response.echoClientTick == 12345);
}

TEST_CASE("The response echoes the request's invoke id") {
    for (uint32_t invokeId : {1u, 42u, 0xFFFFu}) {
        CAPTURE(invokeId);
        InvokeFixture fixture;
        fixture.Receive(InvokeRequest(invokeId, PingId, PingRequest{}));
        std::optional<Reply> reply = fixture.TakeReply();
        REQUIRE(reply);
        CHECK(reply->invoke.invokeId == invokeId);
    }
}

TEST_CASE("A packet shorter than its header is dropped") {
    InvokeFixture fixture;
    fixture.Receive(std::vector<uint8_t>{0x40, 0x00});
    CHECK_FALSE(fixture.TakeReply());
}

TEST_CASE("An unknown procedure gets no response") {
    InvokeFixture fixture;
    uint32_t zero = 0;
    fixture.Receive(InvokeRequestBytes(2, 0xDEAD, &zero, sizeof(zero)));
    CHECK_FALSE(fixture.TakeReply());
}

TEST_CASE("Invoke packets on the unreliable channel are ignored") {
    InvokeFixture fixture;
    fixture.Receive(InvokeRequest(1, PingId, PingRequest{}), NetworkChannel::Unreliable);
    CHECK_FALSE(fixture.TakeReply());
}

TEST_CASE("A request whose payload doesn't parse gets no response") {
    InvokeFixture fixture;
    const char junk[] = "not a valid spawn payload";
    fixture.Receive(InvokeRequestBytes(3, SpawnPlayerId, junk, std::strlen(junk)));
    CHECK_FALSE(fixture.TakeReply());
}

TEST_CASE("A remote invoke resolves when its response arrives") {
    InvokeFixture fixture;
    fixture.network.mode = NetworkMode::Client;
    SpawnPlayerRequest request;
    request.sceneId = 1;
    request.playerName = "TestPlayer";
    Future<SpawnPlayerResponse> future = fixture.invoke.Invoke<SpawnPlayer>(Peer, request);

    std::optional<Reply> sent = fixture.TakeReply();
    REQUIRE(sent);
    CHECK(sent->packet.packetType == static_cast<uint8_t>(PacketType::InvokeRequest));

    SpawnPlayerResponse answer;
    answer.entityId = 9;
    answer.success = true;
    fixture.Receive(PacketWriter()
                        .BeginPacket(PacketType::InvokeResponse)
                        .WriteInvokeHeader(sent->invoke.invokeId, SpawnPlayerId)
                        .Write(answer)
                        .Build());

    REQUIRE(future.IsReady());
    CHECK(future.Get().entityId == 9);
    CHECK(future.Get().success);
}
