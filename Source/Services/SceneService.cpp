#include "Services/SceneService.h"
#include "Services/LogService.h"
#include "tinyxml2.h"
#include "raylib.h"

using namespace tinyxml2;

namespace Elysium::Services {

void SceneService::SetScene(std::unique_ptr<Scene> scene) {
    if (currentScene_) {
        LOG_SERVICE_INFO("SCENE_SERVICE", "Exiting current scene");
        currentScene_->OnExit();
    }
    currentScene_ = std::move(scene);
    if (currentScene_) {
        LOG_SERVICE_INFO("SCENE_SERVICE", "Entering new scene");
        currentScene_->OnEnter();
    }
}

void SceneService::QueueScene(std::unique_ptr<Scene> scene) {
    if (!sceneTransitionLocked_) {
        LOG_SECTION_START("SCENE TRANSITION");
        LOG_SERVICE_INFO("SCENE_SERVICE", "Scene transition queued");
        
        pendingScene_ = std::move(scene);
        sceneTransitionPending_ = true;
        
        // Get assets from the scene but don't start loading yet
        // Loading will start after fade out completes
        if (pendingScene_) {
            pendingAssets_ = pendingScene_->GetAssets();
        }
    }
}

void SceneService::RegisterScene(const std::string& typeName, SceneFactory factory) {
    sceneFactories_[typeName] = factory;
    LOG_SERVICE_INFOF("SCENE_SERVICE", "Registered scene type: %s", typeName.c_str());
}

void SceneService::QueueScene(const std::string& xmlPath) {
    // Read the XML file to determine the scene type
    XMLDocument doc;
    if (doc.LoadFile(xmlPath.c_str()) != XML_SUCCESS) {
        LOG_SERVICE_ERRORF("SCENE_SERVICE", "Failed to load scene file: %s. Error: %s", xmlPath.c_str(), doc.ErrorStr());
        return;
    }
    
    XMLElement *root = doc.FirstChildElement("Scene");
    if (!root) {
        LOG_SERVICE_ERRORF("SCENE_SERVICE", "Invalid scene file format in: %s", xmlPath.c_str());
        return;
    }
    
    const char* sceneType = root->Attribute("type");
    if (!sceneType) {
        LOG_SERVICE_ERRORF("SCENE_SERVICE", "Scene type not specified in: %s", xmlPath.c_str());
        return;
    }
    
    // Find the scene factory
    auto factoryIt = sceneFactories_.find(sceneType);
    if (factoryIt == sceneFactories_.end()) {
        LOG_SERVICE_ERRORF("SCENE_SERVICE", "Unknown scene type '%s' in file: %s", sceneType, xmlPath.c_str());
        return;
    }
    
    // Create the scene using the factory
    std::unique_ptr<Scene> scene = factoryIt->second();
    
    // Load the scene data from XML
    scene->LoadFromXML(xmlPath);
    
    // Queue the scene transition
    QueueScene(std::move(scene));
    LOG_SERVICE_INFOF("SCENE_SERVICE", "Queued scene from XML: %s (type: %s)", xmlPath.c_str(), sceneType);
}

void SceneService::Update(float deltaTime) {
    HandleSceneTransition();
    
    if (transitionState_ == TransitionState::FADE_OUT) {
        transitionTimer_ += deltaTime;
        if (transitionTimer_ >= transitionDuration_) {
            // Fade out complete, now start loading
            transitionState_ = TransitionState::LOADING;
            transitionTimer_ = 0.0f;
        }
    }
    else if (transitionState_ == TransitionState::FADE_IN) {
        transitionTimer_ += deltaTime;
        if (transitionTimer_ >= transitionDuration_) {
            // Fade in complete
            transitionState_ = TransitionState::NONE;
            transitionTimer_ = 0.0f;
            SetScene(std::move(pendingScene_));
            sceneTransitionPending_ = false;
            sceneTransitionLocked_ = false;
            LOG_SECTION_END("SCENE TRANSITION");
        }
    }
    else if (transitionState_ == TransitionState::NONE && currentScene_) {
        currentScene_->OnUpdate(deltaTime);
    }
}

void SceneService::ProcessEvents() {
    // Only process events if we're not transitioning
    if (transitionState_ == TransitionState::NONE && currentScene_) {
        // Event processing is handled by the Application for now
        // This method exists for future expansion when we need scene-specific event handling
    }
}

float SceneService::GetTransitionProgress() const {
    if (transitionState_ == TransitionState::NONE) {
        return 0.0f;
    }
    return transitionTimer_ / transitionDuration_;
}

void SceneService::HandleSceneTransition() {
    if (sceneTransitionPending_ && !sceneTransitionLocked_ && transitionState_ == TransitionState::NONE) {
        sceneTransitionLocked_ = true;
        transitionState_ = TransitionState::FADE_OUT;
        transitionTimer_ = 0.0f;
        
        // Don't call OnEnter() yet - wait until loading completes
    }
}

void SceneService::Shutdown() {
    if (currentScene_) {
        currentScene_->OnExit();
        currentScene_.reset();
    }
    
    pendingScene_.reset();
    sceneFactories_.clear();
    pendingAssets_.clear();
    
    transitionState_ = TransitionState::NONE;
    transitionTimer_ = 0.0f;
    sceneTransitionPending_ = false;
    sceneTransitionLocked_ = false;
}

void SceneService::OnAssetsLoaded() {
    if (transitionState_ == TransitionState::LOADING) {
        // Assets loaded, prepare pending scene and transition to fade in
        if (pendingScene_) {
            pendingScene_->OnEnter();
        }
        
        transitionState_ = TransitionState::FADE_IN;
        transitionTimer_ = 0.0f;
    }
}

} // namespace Elysium::Services