#pragma once

#include <string>

namespace Elysium {

// Describes a game as data: which scene to open first, and where its config
// lives. Everything else (Scenes/, Scripts/, Sprites/, etc.) is resolved
// relative to rootDir once it's installed as the asset root via Path::SetAssetsRoot.
struct ProjectConfig {
    std::string name;
    std::string entryScene;
    std::string configPath = "Config/ApplicationConfig.xml";  // relative to rootDir
    std::string rootDir;                                       // directory containing Project.xml, trailing '/'

    static bool FromXML(const std::string& projectXmlPath, ProjectConfig& out);
};

}  // namespace Elysium
