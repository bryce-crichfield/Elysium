#pragma once

#include <string>

namespace Elysium {

struct CommandLineArgs {
    // Fallback used only when the exe is launched without elysium.ps1 (which
    // always resolves and forwards an explicit --Project= path).
    std::string projectPath = "Projects/HelloWorld/Project.xml";
    bool editor = false;
};

class CommandLine {
   public:
    static CommandLineArgs Parse(int argc, char** argv);
};

}  // namespace Elysium
