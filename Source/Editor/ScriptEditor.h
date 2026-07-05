#pragma once

#include "Editor.h"
#include <cstdint>
#include "TextEditor.h"
#include <string>

namespace Elysium {

class ScriptEditor : public Editor {
public:
    ScriptEditor();

    void Initialize(const ApplicationConfig& config) override;
    void Draw(Application& app) override;

private:
    TextEditor textEditor_;
    std::string statusMessage;
    std::string selectedAssetName;
    int fontSize_ = 24;
    void* font_ = nullptr;
};

}
