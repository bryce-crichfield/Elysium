#include "Scenes/RenderTestScene.h"
#include "Application.h"
#include "Scenes/MenuScene.h"
#include "Systems/RenderSystem.h"
#include "imgui.h"
#include <memory>
#include <cmath>

namespace Elysium::Scenes {

RenderTestScene::RenderTestScene() : Scene("RenderTestScene") {
    cameraSpeed_ = 200.0f;
}

void RenderTestScene::OnEnter() {
    TraceLog(LOG_INFO, "Entering RenderTest Scene");
    
    // Create systems for this scene
    CreateCustomSystems();
    
    // This will be called when the scene starts, but we need screen dimensions
    // Setup will happen in first OnDraw call
}

void RenderTestScene::OnExit() {
    TraceLog(LOG_INFO, "Exiting RenderTest Scene");
}

void RenderTestScene::OnUpdate(float deltaTime) {
    // Call parent update to update all systems
    Scene::OnUpdate(deltaTime);
    
    // Handle our custom input
    HandleInput(deltaTime);
}

void RenderTestScene::OnDraw(Rectangle screen) {
    // Setup on first draw (when we have screen dimensions)
    static bool initialized = false;
    if (!initialized) {
        TraceLog(LOG_INFO, "Initializing RenderTestScene with screen size: %.0fx%.0f", screen.width, screen.height);
        SetupCameras(screen);
        SetupLayers();
        CreateWorldObjects();
        CreateLightObjects();
        CreateUIObjects();
        TraceLog(LOG_INFO, "RenderTestScene initialization complete");
        initialized = true;
    }
    
    // IMPORTANT: Call the parent Scene::OnDraw to render all systems
    Scene::OnDraw(screen);
    
    // Draw overlay text on top of the rendered scene
    DrawText("Render Test Scene - Use WASD to move camera", 10, 10, 20, WHITE);
}

void RenderTestScene::OnDebugDraw() {
    ImGui::Begin("Render Test Scene Manager");
    
    ImGui::Text("Current Scene: %s", GetName().c_str());
    ImGui::Separator();
    
    ImGui::Text("Controls:");
    ImGui::Text("WASD - Move main camera");
    ImGui::SliderFloat("Camera Speed", &cameraSpeed_, 50.0f, 500.0f);
    
    if (ImGui::Button("Back to Menu Scene", ImVec2(200, 30))) {
        auto menuScene = std::make_unique<MenuScene>();
        Elysium::Application::GetInstance().QueueScene(std::move(menuScene));
    }
    
    ImGui::Separator();
    ImGui::Text("Render System Info:");
    ImGui::Text("Main Camera - Full screen viewport");
    ImGui::Text("Minimap Camera - Top-right corner");
    ImGui::Text("World Layer - Squares (z: 0)");
    ImGui::Text("Light Layer - Circles (z: 10)");
    ImGui::Text("UI Layer - Text & Rects (z: 20)");
    
    ImGui::End();
}

void RenderTestScene::SetupCameras(Rectangle screen) {
    // Create main camera entity
    mainCameraEntity_ = world_->CreateEntity("MainCamera");
    world_->AddComponent(mainCameraEntity_, PositionComponent(0.0f, 0.0f));
    
    CameraComponent mainCamera;
    mainCamera.position = {0.0f, 0.0f};
    mainCamera.zoom = 1.0f;
    mainCamera.viewport = {0, 0, screen.width, screen.height};
    mainCamera.renderOrder = 0;
    mainCamera.isVisible = true;
    world_->AddComponent(mainCameraEntity_, mainCamera);
    
    // Create minimap camera entity (top-right corner)
    minimapCameraEntity_ = world_->CreateEntity("MinimapCamera");
    world_->AddComponent(minimapCameraEntity_, PositionComponent(0.0f, 0.0f));
    
    CameraComponent minimapCamera;
    minimapCamera.position = {0.0f, 0.0f};
    minimapCamera.zoom = 0.25f; // Zoomed out for minimap effect
    minimapCamera.viewport = {screen.width - 200, 0, 200, 200}; // Top-right corner
    minimapCamera.renderOrder = 1;
    minimapCamera.isVisible = true;
    world_->AddComponent(minimapCameraEntity_, minimapCamera);
    
    // Create a player entity for camera following
    playerEntity_ = world_->CreateEntity("Player");
    world_->AddComponent(playerEntity_, PositionComponent(0.0f, 0.0f));
}

void RenderTestScene::SetupLayers() {
    // Create World Layer (z: 0)
    worldLayerEntity_ = world_->CreateEntity("WorldLayer");
    LayerComponent worldLayer;
    worldLayer.zIndex = 0;
    worldLayer.type = LayerComponent::Type::World;
    worldLayer.space = LayerComponent::Space::World;
    worldLayer.blend = LayerComponent::Blend::Normal;
    worldLayer.opacity = 1.0f;
    worldLayer.isVisible = true;
    worldLayer.name = "world";
    world_->AddComponent(worldLayerEntity_, worldLayer);
    
    // Create Light Layer (z: 10)
    lightLayerEntity_ = world_->CreateEntity("LightLayer");
    LayerComponent lightLayer;
    lightLayer.zIndex = 10;
    lightLayer.type = LayerComponent::Type::Lighting;
    lightLayer.space = LayerComponent::Space::World;
    lightLayer.blend = LayerComponent::Blend::Additive;
    lightLayer.opacity = 0.8f;
    lightLayer.isVisible = true;
    lightLayer.name = "light";
    world_->AddComponent(lightLayerEntity_, lightLayer);
    
    // Create UI Layer (z: 20)
    uiLayerEntity_ = world_->CreateEntity("UILayer");
    LayerComponent uiLayer;
    uiLayer.zIndex = 20;
    uiLayer.type = LayerComponent::Type::Overlay;
    uiLayer.space = LayerComponent::Space::Screen;
    uiLayer.blend = LayerComponent::Blend::Normal;
    uiLayer.opacity = 1.0f;
    uiLayer.isVisible = true;
    uiLayer.name = "ui";
    world_->AddComponent(uiLayerEntity_, uiLayer);
}

void RenderTestScene::CreateWorldObjects() {
    // Create some squares in the world layer
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            Entity square = world_->CreateEntity("Square_" + std::to_string(i * 10 + j));
            world_->AddComponent(square, PositionComponent(i * 100.0f - 500.0f, j * 100.0f - 500.0f));
            
            Color color = {
                (unsigned char)(100 + (i * 15) % 155),
                (unsigned char)(100 + (j * 15) % 155),
                (unsigned char)(150 + ((i + j) * 10) % 105),
                255
            };
            
            world_->AddComponent(square, RectangleComponent(80.0f, 80.0f, color, WHITE, "world"));
        }
    }
    
    // Add a few larger squares as landmarks
    for (int i = 0; i < 5; i++) {
        Entity landmark = world_->CreateEntity("Landmark_" + std::to_string(i));
        world_->AddComponent(landmark, PositionComponent(i * 300.0f - 600.0f, i * 200.0f - 400.0f));
        world_->AddComponent(landmark, RectangleComponent(150.0f, 150.0f, RED, MAROON, "world"));
    }
}

void RenderTestScene::CreateLightObjects() {
    // Create some light circles
    for (int i = 0; i < 8; i++) {
        Entity light = world_->CreateEntity("Light_" + std::to_string(i));
        float angle = (i * 45.0f) * DEG2RAD;
        float radius = 200.0f;
        world_->AddComponent(light, PositionComponent(cos(angle) * radius, sin(angle) * radius));
        
        Color lightColor = {
            (unsigned char)(100 + (i * 20) % 155),
            (unsigned char)(100 + (i * 30) % 155),
            (unsigned char)(200 + (i * 10) % 55),
            100
        };
        
        world_->AddComponent(light, LightComponent(lightColor, 80.0f, "light"));
    }
    
    // Add some moving lights
    for (int i = 0; i < 3; i++) {
        Entity movingLight = world_->CreateEntity("MovingLight_" + std::to_string(i));
        world_->AddComponent(movingLight, PositionComponent(i * 150.0f - 150.0f, i * 100.0f - 100.0f));
        world_->AddComponent(movingLight, LightComponent(YELLOW, 120.0f, "light"));
    }
}

void RenderTestScene::CreateUIObjects() {
    // Create UI text elements
    Entity titleText = world_->CreateEntity("TitleText");
    world_->AddComponent(titleText, PositionComponent(400.0f, 50.0f));
    world_->AddComponent(titleText, TextComponent("Render Test Scene", 32, WHITE, "ui"));
    
    Entity instructionText = world_->CreateEntity("InstructionText");
    world_->AddComponent(instructionText, PositionComponent(400.0f, 100.0f));
    world_->AddComponent(instructionText, TextComponent("WASD to move camera", 20, LIGHTGRAY, "ui"));
    
    // Create UI rectangles (borders, panels, etc.)
    Entity minimapBorder = world_->CreateEntity("MinimapBorder");
    world_->AddComponent(minimapBorder, PositionComponent(700.0f, 100.0f)); // Adjust based on viewport
    world_->AddComponent(minimapBorder, RectangleComponent(210.0f, 210.0f, BLANK, WHITE, "ui"));
    
    Entity statusPanel = world_->CreateEntity("StatusPanel");
    world_->AddComponent(statusPanel, PositionComponent(100.0f, 600.0f));
    world_->AddComponent(statusPanel, RectangleComponent(200.0f, 100.0f, {0, 0, 0, 150}, GRAY, "ui"));
}

void RenderTestScene::HandleInput(float deltaTime) {
    if (!world_->HasComponent<PositionComponent>(mainCameraEntity_)) {
        return;
    }
    
    auto& cameraPos = world_->GetComponent<PositionComponent>(mainCameraEntity_);
    auto& cameraComp = world_->GetComponent<CameraComponent>(mainCameraEntity_);
    
    Vector2 movement = {0.0f, 0.0f};
    
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
        movement.y -= 1.0f;
    }
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
        movement.y += 1.0f;
    }
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
        movement.x -= 1.0f;
    }
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
        movement.x += 1.0f;
    }
    
    // Normalize diagonal movement
    if (movement.x != 0.0f && movement.y != 0.0f) {
        float length = sqrt(movement.x * movement.x + movement.y * movement.y);
        movement.x /= length;
        movement.y /= length;
    }
    
    // Apply movement
    cameraComp.position.x += movement.x * cameraSpeed_ * deltaTime;
    cameraComp.position.y += movement.y * cameraSpeed_ * deltaTime;
    
    // Update minimap camera to follow main camera (but zoomed out)
    if (world_->HasComponent<CameraComponent>(minimapCameraEntity_)) {
        auto& minimapComp = world_->GetComponent<CameraComponent>(minimapCameraEntity_);
        minimapComp.position = cameraComp.position;
    }
}

void RenderTestScene::CreateCustomSystems() {
    // Create context for systems
    Context context = Context {
        .application = &Application::GetInstance(),
        .scene = this,
        .world = world_.get()
    };
    
    // Create RenderSystem - this is essential for rendering our entities
    systems_.push_back(std::make_unique<Systems::RenderSystem>(context));
    TraceLog(LOG_INFO, "Created RenderSystem for RenderTestScene");
}

} // namespace Elysium::Scenes