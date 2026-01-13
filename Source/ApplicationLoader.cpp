#include "Application.h"
#include "Path.h"
#include "raylib.h"
#include "tinyxml2.h"

using namespace tinyxml2;

namespace Elysium
{

bool ApplicationConfig::FromXML(const std::string &configPath, ApplicationConfig &out)
{
    ApplicationConfig &config = out;
    XMLDocument doc;

    if (doc.LoadFile(Path(configPath).c_str()) != XML_SUCCESS)
    {
        TraceLog(LOG_ERROR, "Failed to load config file: %s. Using defaults.", configPath.c_str());
        return false;
    }

    XMLElement *root = doc.FirstChildElement("GameConfig");
    if (!root)
    {
        TraceLog(LOG_ERROR, "Invalid config file format. Using defaults.");
        return false;
    }

    if (XMLElement *window = root->FirstChildElement("Window"))
    {
        if (XMLElement *width = window->FirstChildElement("Width"))
            config.windowWidth = width->IntText(1280);
        if (XMLElement *height = window->FirstChildElement("Height"))
            config.windowHeight = height->IntText(720);
        if (XMLElement *title = window->FirstChildElement("Title"))
            config.windowTitle = title->GetText() ? title->GetText() : "Elysium";
        if (XMLElement *fullscreen = window->FirstChildElement("Fullscreen"))
            config.fullscreen = fullscreen->BoolText(false);
        if (XMLElement *vsync = window->FirstChildElement("VSync"))
            config.vsync = vsync->BoolText(true);
        if (XMLElement *fps = window->FirstChildElement("TargetFPS"))
            config.targetFPS = fps->IntText(60);
        if (XMLElement *backgroundColor = window->FirstChildElement("BackgroundColor"))
        {
            config.backgroundColor = BLACK;
            if (XMLElement *r = backgroundColor->FirstChildElement("r"))
                config.backgroundColor.r = r->IntText(255);
            if (XMLElement *g = backgroundColor->FirstChildElement("g"))
                config.backgroundColor.g = g->IntText(255);
            if (XMLElement *b = backgroundColor->FirstChildElement("b"))
                config.backgroundColor.b = b->IntText(255);

            TraceLog(LOG_INFO, "Color: %d, %d, %d", config.backgroundColor.r, config.backgroundColor.g,
                     config.backgroundColor.b);
        }
        if (XMLElement *framebuffer = window->FirstChildElement("Framebuffer"))
        {
            if (XMLElement *width = framebuffer->FirstChildElement("Width"))
                config.framebufferWidth = width->IntText(640);
            if (XMLElement *height = framebuffer->FirstChildElement("Height"))
                config.framebufferHeight = height->IntText(480);
        }
    }

    if (XMLElement *debug = root->FirstChildElement("Debug"))
    {
        if (XMLElement *showDemo = debug->FirstChildElement("ShowDemoWindow"))
            config.showDemoWindow = showDemo->BoolText(true);
        if (XMLElement *showMetrics = debug->FirstChildElement("ShowMetrics"))
            config.showMetrics = showMetrics->BoolText(false);
        if (XMLElement *logLevel = debug->FirstChildElement("LogLevel"))
            config.logLevel = logLevel->GetText() ? logLevel->GetText() : "INFO";
    }

    TraceLog(LOG_INFO, "Loaded game config from: %s", configPath.c_str());

    return true;
}

} // namespace Elysium
