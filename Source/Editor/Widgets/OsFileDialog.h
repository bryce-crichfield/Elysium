#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Elysium {

// The OS's own open-file dialog. It runs alongside the editor rather than blocking the frame,
// so the window keeps drawing while it's up.
class OsOpenFileDialog {
   public:
    OsOpenFileDialog();
    ~OsOpenFileDialog();

    // `filters` pairs a label with its patterns: {"Project", "Project.xml"}.
    void Open(const std::string& title, const std::string& startPath, const std::vector<std::string>& filters);
    bool IsOpen() const { return pending_ != nullptr; }

    // The chosen file, on the frame the dialog closes with one; nothing while it's open or if cancelled.
    std::optional<std::string> Poll();

   private:
    struct Pending;
    std::unique_ptr<Pending> pending_;
};

}  // namespace Elysium
