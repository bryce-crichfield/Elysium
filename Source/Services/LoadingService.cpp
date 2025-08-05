#include "Services/LoadingService.h"
#include "Services/AssetService.h"
#include "raylib.h"
#include "tinyxml2.h"
#include <chrono>
#include <sstream>
#include <regex>
#include <cmath>

using namespace tinyxml2;

namespace Elysium::Services {

void LoadingService::Initialize()
{
    shouldExit_ = false;
    LoadConfig("./Assets/LoadingConfig.xml");
}

void LoadingService::Shutdown()
{
    shouldExit_ = true;
    if (loadingThread_.joinable()) {
        loadingThread_.join();
    }
    
    // Unload background textures
    for (auto& texture : backgroundTextures_) {
        if (texture.id != 0) {
            UnloadTexture(texture);
        }
    }
    backgroundTextures_.clear();
    
    // Unload background music
    if (musicLoaded_) {
        UnloadSound(backgroundMusic_);
        musicLoaded_ = false;
    }
}

bool LoadingService::IsLoading() const
{
    return isLoading_.load();
}

float LoadingService::GetProgress() const
{
    int total = totalAssets_.load();
    if (total == 0) return 1.0f;
    
    int loaded = loadedAssets_.load();
    return (float)loaded / (float)total;
}

int LoadingService::GetTotalAssets() const
{
    return totalAssets_.load();
}

int LoadingService::GetLoadedAssets() const
{
    return loadedAssets_.load();
}

void LoadingService::LoadAssets(const std::vector<Asset>& assets, AssetService& assetService)
{
    if (isLoading_.load()) {
        return; // Already loading
    }
    
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        assetQueue_ = assets;
        assetService_ = &assetService;
        totalAssets_.store(assets.size());
        loadedAssets_.store(0);
    }
    
    isLoading_.store(true);
    
    // Start loading thread if not already running
    if (loadingThread_.joinable()) {
        loadingThread_.join();
    }
    
    loadingThread_ = std::thread(&LoadingService::LoadingThreadFunction, this);
}

void LoadingService::ClearQueue()
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    assetQueue_.clear();
    totalAssets_.store(0);
    loadedAssets_.store(0);
}

void LoadingService::LoadingThreadFunction()
{
    TraceLog(LOG_INFO, "Loading thread started");
    
    std::vector<Asset> assetsToLoad;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        assetsToLoad = assetQueue_;
    }
    
    for (const auto& asset : assetsToLoad) {
        if (shouldExit_) {
            break;
        }
        
        if (assetService_) {
            TraceLog(LOG_INFO, "Loading asset: %s -> %s", asset.GetName().c_str(), asset.GetPath().c_str());
            assetService_->LoadAsset(asset);
            loadedAssets_.fetch_add(1);
            
            int delayTime = config_.delayTime;  // notice, prob not thread safe eggggaddd
            std::this_thread::sleep_for(std::chrono::milliseconds(delayTime));
        }
    }
    
    isLoading_.store(false);
    TraceLog(LOG_INFO, "Loading thread finished");
}

void LoadingService::LoadConfig(const std::string& configPath)
{
    TraceLog(LOG_INFO, "Attempting to load LoadingConfig from: %s", configPath.c_str());
    
    XMLDocument doc;
    XMLError result = doc.LoadFile(configPath.c_str());
    if (result != XML_SUCCESS) {
        TraceLog(LOG_WARNING, "Failed to load LoadingConfig.xml, error code: %d", result);
        TraceLog(LOG_WARNING, "XML Error: %s", doc.ErrorStr());
        if (doc.ErrorStr()) {
            TraceLog(LOG_WARNING, "XML Error details: %s", doc.ErrorStr());
        }
        return;
    }
    
    
    XMLElement* root = doc.FirstChildElement("LoadingConfig");
    if (!root) {
        TraceLog(LOG_WARNING, "Invalid LoadingConfig.xml format - no LoadingConfig root element");
        return;
    }

    if (XMLElement* delayTime = root->FirstChildElement("DelayTime")) {
        config_.delayTime = delayTime->IntText(100);
    }

    // Load Progress Bar config
    if (XMLElement* progressBar = root->FirstChildElement("ProgressBar")) {
        if (XMLElement* width = progressBar->FirstChildElement("Width"))
            config_.progressBar.width = width->IntText(400);
        if (XMLElement* height = progressBar->FirstChildElement("Height"))
            config_.progressBar.height = height->IntText(25);
        if (XMLElement* x = progressBar->FirstChildElement("X"))
            config_.progressBar.x = x->IntText(-1);
        if (XMLElement* y = progressBar->FirstChildElement("Y"))
            config_.progressBar.y = y->IntText(300);
            
        if (XMLElement* bgColor = progressBar->FirstChildElement("BackgroundColor")) {
            config_.progressBar.backgroundColor.r = bgColor->FirstChildElement("r")->IntText(64);
            config_.progressBar.backgroundColor.g = bgColor->FirstChildElement("g")->IntText(64);
            config_.progressBar.backgroundColor.b = bgColor->FirstChildElement("b")->IntText(64);
            config_.progressBar.backgroundColor.a = bgColor->FirstChildElement("a")->IntText(255);
        }
        
        if (XMLElement* borderColor = progressBar->FirstChildElement("BorderColor")) {
            config_.progressBar.borderColor.r = borderColor->FirstChildElement("r")->IntText(128);
            config_.progressBar.borderColor.g = borderColor->FirstChildElement("g")->IntText(128);
            config_.progressBar.borderColor.b = borderColor->FirstChildElement("b")->IntText(128);
            config_.progressBar.borderColor.a = borderColor->FirstChildElement("a")->IntText(255);
        }
        
        if (XMLElement* fillColor = progressBar->FirstChildElement("FillColor")) {
            config_.progressBar.fillColor.r = fillColor->FirstChildElement("r")->IntText(0);
            config_.progressBar.fillColor.g = fillColor->FirstChildElement("g")->IntText(255);
            config_.progressBar.fillColor.b = fillColor->FirstChildElement("b")->IntText(128);
            config_.progressBar.fillColor.a = fillColor->FirstChildElement("a")->IntText(255);
        }
        
        if (XMLElement* borderThickness = progressBar->FirstChildElement("BorderThickness"))
            config_.progressBar.borderThickness = borderThickness->IntText(2);
    }
    
    // Load Text config
    if (XMLElement* text = root->FirstChildElement("Text")) {
        if (XMLElement* loadingText = text->FirstChildElement("LoadingText"))
            config_.text.loadingText = loadingText->GetText() ? loadingText->GetText() : "Loading...";
        if (XMLElement* fontSize = text->FirstChildElement("FontSize"))
            config_.text.fontSize = fontSize->IntText(48);
        if (XMLElement* x = text->FirstChildElement("X"))
            config_.text.x = x->IntText(-1);
        if (XMLElement* y = text->FirstChildElement("Y"))
            config_.text.y = y->IntText(200);
            
        if (XMLElement* color = text->FirstChildElement("Color")) {
            config_.text.color.r = color->FirstChildElement("r")->IntText(255);
            config_.text.color.g = color->FirstChildElement("g")->IntText(255);
            config_.text.color.b = color->FirstChildElement("b")->IntText(255);
            config_.text.color.a = color->FirstChildElement("a")->IntText(255);
        }
        
        if (XMLElement* statusText = text->FirstChildElement("StatusText"))
            config_.text.statusText = statusText->GetText() ? statusText->GetText() : "Assets loaded: {loaded} / {total}";
        if (XMLElement* statusFontSize = text->FirstChildElement("StatusFontSize"))
            config_.text.statusFontSize = statusFontSize->IntText(24);
        if (XMLElement* statusX = text->FirstChildElement("StatusX"))
            config_.text.statusX = statusX->IntText(-1);
        if (XMLElement* statusY = text->FirstChildElement("StatusY"))
            config_.text.statusY = statusY->IntText(350);
            
        if (XMLElement* statusColor = text->FirstChildElement("StatusColor")) {
            config_.text.statusColor.r = statusColor->FirstChildElement("r")->IntText(200);
            config_.text.statusColor.g = statusColor->FirstChildElement("g")->IntText(200);
            config_.text.statusColor.b = statusColor->FirstChildElement("b")->IntText(200);
            config_.text.statusColor.a = statusColor->FirstChildElement("a")->IntText(255);
        }
    }
    
    // Load Background config
    if (XMLElement* background = root->FirstChildElement("Background")) {
        if (XMLElement* images = background->FirstChildElement("Images")) {
            for (XMLElement* image = images->FirstChildElement("Image"); image != nullptr; image = image->NextSiblingElement("Image")) {
                if (image->GetText()) {
                    config_.background.imagePaths.push_back(image->GetText());
                }
            }
        }
        
        if (XMLElement* cycleTime = background->FirstChildElement("CycleTime"))
            config_.background.cycleTime = cycleTime->FloatText(2.0f);
            
        if (XMLElement* music = background->FirstChildElement("Music"))
            config_.background.musicPath = music->GetText() ? music->GetText() : "";
    }
    
    // Load Tooltips config
    TraceLog(LOG_INFO, "Parsing Tooltips config...");  
    if (XMLElement* tooltips = root->FirstChildElement("Tooltips")) {
        TraceLog(LOG_INFO, "Found Tooltips element");
        if (XMLElement* messages = tooltips->FirstChildElement("Messages")) {
            for (XMLElement* message = messages->FirstChildElement("Message"); message != nullptr; message = message->NextSiblingElement("Message")) {
                if (message->GetText()) {
                    config_.tooltips.messages.push_back(message->GetText());
                }
            }
        }
        
        if (XMLElement* cycleTime = tooltips->FirstChildElement("CycleTime"))
            config_.tooltips.cycleTime = cycleTime->FloatText(3.0f);
        if (XMLElement* fontSize = tooltips->FirstChildElement("FontSize"))
            config_.tooltips.fontSize = fontSize->IntText(20);
        if (XMLElement* x = tooltips->FirstChildElement("X"))
            config_.tooltips.x = x->IntText(-1);
        if (XMLElement* y = tooltips->FirstChildElement("Y"))
            config_.tooltips.y = y->IntText(400);
            
        if (XMLElement* color = tooltips->FirstChildElement("Color")) {
            if (XMLElement* r = color->FirstChildElement("r"))
                config_.tooltips.color.r = r->IntText(180);
            if (XMLElement* g = color->FirstChildElement("g"))
                config_.tooltips.color.g = g->IntText(180);
            if (XMLElement* b = color->FirstChildElement("b"))
                config_.tooltips.color.b = b->IntText(180);
            if (XMLElement* a = color->FirstChildElement("a"))
                config_.tooltips.color.a = a->IntText(255);
        }
    } else {
        TraceLog(LOG_INFO, "No Tooltips element found in config");
    }
    
    // Load background textures
    for (const auto& imagePath : config_.background.imagePaths) {
        Texture2D texture = LoadTexture(imagePath.c_str());
        if (texture.id != 0) {
            backgroundTextures_.push_back(texture);
        } else {
            TraceLog(LOG_WARNING, "Failed to load loading background: %s", imagePath.c_str());
        }
    }
    
    // Load background music
    if (!config_.background.musicPath.empty()) {
        backgroundMusic_ = LoadSound(config_.background.musicPath.c_str());
        if (backgroundMusic_.frameCount > 0) {
            musicLoaded_ = true;
        } else {
            TraceLog(LOG_WARNING, "Failed to load loading music: %s", config_.background.musicPath.c_str());
        }
    }
    
    TraceLog(LOG_INFO, "LoadingConfig.xml loaded and parsed successfully");
}

void LoadingService::Draw(int screenWidth, int screenHeight)
{
    // Update background timer
    backgroundTimer_ += GetFrameTime();
    if (backgroundTimer_ >= config_.background.cycleTime && !backgroundTextures_.empty()) {
        backgroundTimer_ = 0.0f;
        currentBackgroundIndex_ = (currentBackgroundIndex_ + 1) % backgroundTextures_.size();
    }
    
    // Update tooltip timer
    tooltipTimer_ += GetFrameTime();
    if (tooltipTimer_ >= config_.tooltips.cycleTime && !config_.tooltips.messages.empty()) {
        tooltipTimer_ = 0.0f;
        currentTooltipIndex_ = (currentTooltipIndex_ + 1) % config_.tooltips.messages.size();
    }
    
    DrawBackground(screenWidth, screenHeight);
    DrawLoadingText(screenWidth, screenHeight);
    DrawProgressBar(screenWidth, screenHeight);
    DrawTooltips(screenWidth, screenHeight);
}

void LoadingService::DrawBackground(int screenWidth, int screenHeight)
{
    // Clear to black first
    ClearBackground(BLACK);
    
    // Draw background image if available
    if (!backgroundTextures_.empty() && currentBackgroundIndex_ < backgroundTextures_.size()) {
        Texture2D& texture = backgroundTextures_[currentBackgroundIndex_];
        if (texture.id != 0) {
            // Scale to fit screen while maintaining aspect ratio
            float scaleX = (float)screenWidth / texture.width;
            float scaleY = (float)screenHeight / texture.height;
            float scale = fminf(scaleX, scaleY);
            
            int scaledWidth = (int)(texture.width * scale);
            int scaledHeight = (int)(texture.height * scale);
            int offsetX = (screenWidth - scaledWidth) / 2;
            int offsetY = (screenHeight - scaledHeight) / 2;
            
            DrawTexturePro(
                texture,
                Rectangle{0, 0, (float)texture.width, (float)texture.height},
                Rectangle{(float)offsetX, (float)offsetY, (float)scaledWidth, (float)scaledHeight},
                Vector2{0, 0},
                0.0f,
                WHITE
            );
        }
    }
}

void LoadingService::DrawLoadingText(int screenWidth, int screenHeight)
{
    // Draw loading text
    const char* loadingText = config_.text.loadingText.c_str();
    int textWidth = MeasureText(loadingText, config_.text.fontSize);
    int textX = config_.text.x == -1 ? (screenWidth - textWidth) / 2 : config_.text.x;
    ::DrawText(loadingText, textX, config_.text.y, config_.text.fontSize, config_.text.color);
    
    // Draw status text
    std::string statusText = FormatStatusText(config_.text.statusText, loadedAssets_.load(), totalAssets_.load());
    int statusWidth = MeasureText(statusText.c_str(), config_.text.statusFontSize);
    int statusX = config_.text.statusX == -1 ? (screenWidth - statusWidth) / 2 : config_.text.statusX;
    ::DrawText(statusText.c_str(), statusX, config_.text.statusY, config_.text.statusFontSize, config_.text.statusColor);
}

void LoadingService::DrawProgressBar(int screenWidth, int screenHeight)
{
    int barX = config_.progressBar.x == -1 ? (screenWidth - config_.progressBar.width) / 2 : config_.progressBar.x;
    int barY = config_.progressBar.y;
    
    // Draw background
    DrawRectangle(barX, barY, config_.progressBar.width, config_.progressBar.height, config_.progressBar.backgroundColor);
    
    // Draw progress fill
    float progress = GetProgress();
    int fillWidth = (int)(config_.progressBar.width * progress);
    if (fillWidth > 0) {
        DrawRectangle(barX, barY, fillWidth, config_.progressBar.height, config_.progressBar.fillColor);
    }
    
    // Draw border
    if (config_.progressBar.borderThickness > 0) {
        DrawRectangleLines(barX, barY, config_.progressBar.width, config_.progressBar.height, config_.progressBar.borderColor);
        if (config_.progressBar.borderThickness > 1) {
            for (int i = 1; i < config_.progressBar.borderThickness; i++) {
                DrawRectangleLines(barX - i, barY - i, config_.progressBar.width + 2*i, config_.progressBar.height + 2*i, config_.progressBar.borderColor);
            }
        }
    }
}

std::string LoadingService::FormatStatusText(const std::string& template_str, int loaded, int total)
{
    std::string result = template_str;
    
    // Replace {loaded} with actual loaded count
    size_t pos = result.find("{loaded}");
    if (pos != std::string::npos) {
        result.replace(pos, 8, std::to_string(loaded));
    }
    
    // Replace {total} with actual total count
    pos = result.find("{total}");
    if (pos != std::string::npos) {
        result.replace(pos, 7, std::to_string(total));
    }
    
    return result;
}

void LoadingService::DrawTooltips(int screenWidth, int screenHeight)
{
    // Draw tooltip if we have messages
    if (!config_.tooltips.messages.empty() && currentTooltipIndex_ < config_.tooltips.messages.size()) {
        const char* tooltipText = config_.tooltips.messages[currentTooltipIndex_].c_str();
        int textWidth = MeasureText(tooltipText, config_.tooltips.fontSize);
        int textX = config_.tooltips.x == -1 ? (screenWidth - textWidth) / 2 : config_.tooltips.x;
        ::DrawText(tooltipText, textX, config_.tooltips.y, config_.tooltips.fontSize, config_.tooltips.color);
    }
}

} // namespace Elysium::Services