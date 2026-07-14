#include "Core/Project.h"
#include "Core/Xml.h"
#include "raylib.h"
#include <filesystem>

namespace Elysium {

bool ProjectConfig::FromXML(const std::string& projectXmlPath, ProjectConfig& out) {
    tinyxml2::XMLDocument doc;

    // Loaded via the raw filesystem path, not Path/ASSETS_PATH — this file is
    // what determines the asset root, so it can't depend on one existing yet.
    if (!LoadXml(projectXmlPath, doc)) {
        TraceLog(LOG_ERROR, "Failed to load project file: %s", projectXmlPath.c_str());
        return false;
    }

    tinyxml2::XMLElement* root = doc.FirstChildElement("Project");
    if (!root) {
        TraceLog(LOG_ERROR, "Invalid project file format. Missing root tag <Project>.");
        return false;
    }

    out.name = root->Attribute("name") ? root->Attribute("name") : "";

    if (tinyxml2::XMLElement* entryScene = root->FirstChildElement("EntryScene")) {
        out.entryScene = entryScene->GetText() ? entryScene->GetText() : "";
    }
    if (out.entryScene.empty()) {
        TraceLog(LOG_ERROR, "Project file missing <EntryScene>: %s", projectXmlPath.c_str());
        return false;
    }

    if (tinyxml2::XMLElement* config = root->FirstChildElement("Config")) {
        out.configPath = config->GetText() ? config->GetText() : out.configPath;
    }

    std::filesystem::path dir = std::filesystem::path(projectXmlPath).parent_path();
    out.rootDir = dir.empty() ? "./" : dir.generic_string() + "/";

    TraceLog(LOG_INFO, "Loaded project '%s' from: %s", out.name.c_str(), projectXmlPath.c_str());

    return true;
}

}  // namespace Elysium
