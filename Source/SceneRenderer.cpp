#include "SceneRenderer.h"
#include "Application.h"
#include "Scene.h"
#include "Services/SceneService.h"
#include "Common.h"

namespace Elysium {

void SceneRenderer::Initialize(int framebufferWidth, int framebufferHeight)
{
    Profile;
    framebuffer_ = LoadRenderTexture(framebufferWidth, framebufferHeight);
    CalculateLetterboxing();
}

void SceneRenderer::Shutdown()
{
    Profile;
    UnloadRenderTexture(framebuffer_);
}

void SceneRenderer::OnWindowResized()
{
    CalculateLetterboxing();
}

void SceneRenderer::CalculateLetterboxing()
{
    auto& app = Application::GetInstance();
    const auto& config = app.GetConfig();

    int windowWidth = GetScreenWidth();
    int windowHeight = GetScreenHeight();

    float screenAspect = (float)windowWidth / windowHeight;
    float framebufferAspect = (float)config.framebufferWidth / config.framebufferHeight;

    if (framebufferAspect > screenAspect)
    {
        scaleX_ = scaleY_ = (float)windowWidth / config.framebufferWidth;
        float scaledHeight = config.framebufferHeight * scaleY_;
        offset_.x = 0;
        offset_.y = (windowHeight - scaledHeight) * 0.5f;
        letterboxRect_ = Rectangle{offset_.x, offset_.y, (float)windowWidth, scaledHeight};
    }
    else
    {
        scaleX_ = scaleY_ = (float)windowHeight / config.framebufferHeight;
        float scaledWidth = config.framebufferWidth * scaleX_;
        offset_.x = (windowWidth - scaledWidth) * 0.5f;
        offset_.y = 0;
        letterboxRect_ = Rectangle{offset_.x, offset_.y, scaledWidth, (float)windowHeight};
    }
}

void SceneRenderer::Render(Scene* scene, Services::SceneState state, float transitionProgress, bool isTransitioning, Color backgroundColor)
{
    Profile;
    auto& app = Application::GetInstance();
    const auto& config = app.GetConfig();

    // Recalculate letterboxing if window was resized
    if (IsWindowResized())
    {
        CalculateLetterboxing();
    }

    auto screenRect = Rectangle{0, 0, (float)config.framebufferWidth, (float)config.framebufferHeight};

    if (isTransitioning)
    {
        if (state == Services::SceneState::LOADING_ASSETS)
        {
            // Just show black during asset loading
            ClearBackground(BLACK);
        }
        else
        {
            // Render current scene to framebuffer
            BeginTextureMode(framebuffer_);
            ClearBackground(backgroundColor);
            if (scene)
            {
                scene->OnDraw(screenRect);
            }
            EndTextureMode();

            // Draw framebuffer with transition alpha
            if (state == Services::SceneState::EXITING)
            {
                Color currentTint = {255, 255, 255, (unsigned char)(255 * (1.0f - transitionProgress))};
                DrawTexturePro(
                    framebuffer_.texture,
                    Rectangle{0, 0, (float)framebuffer_.texture.width, -(float)framebuffer_.texture.height},
                    letterboxRect_, Vector2{0, 0}, 0.0f, currentTint);
            }
            else if (state == Services::SceneState::ENTERING)
            {
                Color pendingTint = {255, 255, 255, (unsigned char)(255 * transitionProgress)};
                DrawTexturePro(
                    framebuffer_.texture,
                    Rectangle{0, 0, (float)framebuffer_.texture.width, -(float)framebuffer_.texture.height},
                    letterboxRect_, Vector2{0, 0}, 0.0f, pendingTint);
            }
        }
    }
    else
    {
        // Normal rendering
        BeginTextureMode(framebuffer_);
        ClearBackground(backgroundColor);
        if (scene)
        {
            scene->OnDraw(screenRect);
        }
        EndTextureMode();

        DrawTexturePro(
            framebuffer_.texture,
            Rectangle{0, 0, (float)framebuffer_.texture.width, -(float)framebuffer_.texture.height},
            letterboxRect_, Vector2{0, 0}, 0.0f, WHITE);
    }
}

} // namespace Elysium
