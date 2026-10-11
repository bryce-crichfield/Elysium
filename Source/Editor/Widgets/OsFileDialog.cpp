#include "Editor/Widgets/OsFileDialog.h"

// Kept to this file: on Windows it brings in <windows.h>, whose names collide with raylib's.
#include "portable-file-dialogs.h"

namespace Elysium {

struct OsOpenFileDialog::Pending {
    pfd::open_file dialog;
};

OsOpenFileDialog::OsOpenFileDialog() = default;
OsOpenFileDialog::~OsOpenFileDialog() = default;

void OsOpenFileDialog::Open(const std::string& title, const std::string& startPath, const std::vector<std::string>& filters) {
    if (pending_) return;
    pending_.reset(new Pending{pfd::open_file(title, startPath, filters)});
}

std::optional<std::string> OsOpenFileDialog::Poll() {
    if (!pending_ || !pending_->dialog.ready(0)) return std::nullopt;
    const std::vector<std::string> files = pending_->dialog.result();
    pending_.reset();
    if (files.empty()) return std::nullopt;
    return files.front();
}

}  // namespace Elysium
