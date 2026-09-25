#pragma once

#include "Editor.h"
#include <cstdint>
#include "TextEditor.h"
#include <string>

namespace Elysium {

class ScriptEditor : public Editor {
public:
    static constexpr const char* Title = "Scripts";

    explicit ScriptEditor(ServiceLocator& services);

    void Initialize(const ApplicationConfig& config) override;
    void Draw() override;

private:
    void DrawToolbar();
    void SelectScript(const std::string& name);
    void SaveScript();
    void SetStatus(const std::string& message, bool isError = false);

    TextEditor textEditor_;
    std::string statusMessage_;
    bool statusIsError_ = false;
    std::string selectedAssetName_;
    int fontSize_ = 24;
    void* font_ = nullptr;
};

}
