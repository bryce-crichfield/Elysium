#pragma once

#include <string>
#include "../Network/NetworkProtocol.h"
#include "Message.h"

namespace Elysium::Services {

/**
 * SceneChangedMessage - Published when scene stack changes
 *
 * NetworkService subscribes to this to broadcast scene changes to clients.
 * Other systems can also subscribe if they need to react to scene changes.
 */
struct SceneChangedMessage : public Message {
    Network::SceneChangeOp operation;
    std::string sceneName;

    SceneChangedMessage(Network::SceneChangeOp op, const std::string& name = "")
        : operation(op), sceneName(name) {}
};

}  // namespace Elysium::Services
