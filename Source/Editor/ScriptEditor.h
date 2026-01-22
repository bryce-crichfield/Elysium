#pragma once

#include "Editor.h"
#include <string>

namespace Elysium {

class ScriptEditor : public Editor {
public:
    ScriptEditor();
    void Draw(Application& app) override;

private:
    char scriptPath[256] = "Scripts/test.lua";
    char scriptBuffer[1024 * 64]; // 64KB buffer
    std::string statusMessage;
};

}
