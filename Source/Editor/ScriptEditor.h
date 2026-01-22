#pragma once

#include "Editor.h"
#include <string>

namespace Elysium {

class ScriptEditor : public Editor {
public:
    ScriptEditor();

    void Initialize() override;
    void Draw(Application& app) override;

private:
    char scriptPath[256] = "Scripts/test.lua";
    char scriptBuffer[1024 * 64]; // 64KB buffer
    std::string statusMessage;
    int fontSize_ = 24;
    void* font_ = nullptr;
};

}
