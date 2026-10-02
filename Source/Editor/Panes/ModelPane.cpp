#include <algorithm>
#include <cmath>
#include <filesystem>

#include "Core/Graphics.h"
#include "Core/Path.h"
#include "Editor/Panes/ContentPane.h"
#include "Editor/Widgets/Widgets.h"
#include "Interfaces/IAssetService.h"
#include "Interfaces/IEditorService.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

namespace Elysium {

namespace {

// A key light over the camera's shoulder and a sky fill, enough to read a model's shape.
const char* kPreviewVertex = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matNormal;
out vec2 fragTexCoord;
out vec3 fragNormal;
void main()
{
    fragTexCoord = vertexTexCoord;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* kPreviewFragment = R"(#version 330
in vec2 fragTexCoord;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 uKey;
out vec4 finalColor;
void main()
{
    vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse;
    if (albedo.a < 0.5) discard;
    vec3 n = normalize(fragNormal);
    if (!gl_FrontFacing) n = -n;
    float key = max(dot(n, uKey), 0.0);
    float sky = 0.5 + 0.5 * n.y;
    finalColor = vec4(albedo.rgb * (0.25 + 0.25 * sky + 0.75 * key), albedo.a);
}
)";

::Shader& PreviewShader() {
    static ::Shader shader = LoadShaderFromMemory(kPreviewVertex, kPreviewFragment);
    return shader;
}

// A model preview: the model, shaded, on a slow turntable, framed to its bounds. Drag to turn
// it by hand. Nothing to edit, so nothing to save.
class ModelPane : public ContentPane {
   public:
    ModelPane(ServiceLocator& services, const Services::EditorDocument& document)
        : services_(services), fullPath_(document.fullPath), path_(Path::FromFullPath(document.fullPath)) {
        services_.Get<Services::IAssetService>().LoadAsset<Model>(path_);
    }

    ~ModelPane() override {
        if (target_.id != 0) UnloadRenderTexture(target_);
    }

    void DrawToolbar() override {
        if (ToggleIconButton(ICON_FA_ROTATE, spin_, "Turntable")) spin_ = !spin_;
        if (const Model* model = services_.Get<Services::IAssetService>().Get<Model>(path_)) {
            char info[96];
            snprintf(info, sizeof(info), "%d meshes   %.2f x %.2f x %.2f", model->meshCount,
                     model->boundsMax[0] - model->boundsMin[0], model->boundsMax[1] - model->boundsMin[1],
                     model->boundsMax[2] - model->boundsMin[2]);
            AlignRight(ImGui::CalcTextSize(info).x);
            ImGui::AlignTextToFramePadding();
            ColoredText(Editor::Palette().TextMuted, info);
        }
    }

    void Draw() override {
        const Model* model = services_.Get<Services::IAssetService>().Get<Model>(path_);
        if (!model || !model->native) {
            EmptyState("Loading...");
            return;
        }

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const int width = std::max(1, (int)avail.x), height = std::max(1, (int)avail.y);
        ImGui::InvisibleButton("##preview", avail);
        if (ImGui::IsItemActive()) angle_ += ImGui::GetIO().MouseDelta.x * 0.01f;
        else if (spin_) angle_ += ImGui::GetIO().DeltaTime * 0.6f;

        if (target_.id == 0 || target_.texture.width != width || target_.texture.height != height) {
            if (target_.id != 0) UnloadRenderTexture(target_);
            target_ = LoadRenderTexture(width, height);
        }

        const ::Vector3 min{model->boundsMin[0], model->boundsMin[1], model->boundsMin[2]};
        const ::Vector3 max{model->boundsMax[0], model->boundsMax[1], model->boundsMax[2]};
        // Stood on the grid: footprint centred on the origin, base at y 0 (as a centered
        // ModelComponent places it), so it turns about its own middle.
        const ::Vector3 offset{-(min.x + max.x) * 0.5f, -min.y, -(min.z + max.z) * 0.5f};
        const ::Vector3 center{0.0f, (max.y - min.y) * 0.5f, 0.0f};
        const float radius = 0.5f * std::sqrt((max.x - min.x) * (max.x - min.x) + (max.y - min.y) * (max.y - min.y) +
                                              (max.z - min.z) * (max.z - min.z));
        constexpr float kFov = 40.0f, kPitch = 0.45f;
        const float distance = std::max(radius, 0.001f) / std::sin(kFov * 0.5f * DEG2RAD) * 1.1f;

        ::Camera3D camera{};
        camera.target = center;
        camera.position = {center.x + distance * std::cos(kPitch) * std::sin(angle_), center.y + distance * std::sin(kPitch),
                           center.z + distance * std::cos(kPitch) * std::cos(angle_)};
        camera.up = {0.0f, 1.0f, 0.0f};
        camera.fovy = kFov;
        camera.projection = CAMERA_PERSPECTIVE;

        BeginTextureMode(target_);
        ClearBackground(::Color{24, 22, 30, 255});
        BeginMode3D(camera);
        DrawGrid(10, radius * 0.25f);
        rlDisableBackfaceCulling();  // imported models aren't always consistently wound (as Render3D)
        {
            // The key light turns with the camera, from over its left shoulder.
            ::Shader& shader = PreviewShader();
            const ::Vector3 toCamera = Vector3Normalize(Vector3Subtract(camera.position, center));
            const ::Vector3 right = Vector3Normalize(Vector3CrossProduct(camera.up, toCamera));
            const ::Vector3 key = Vector3Normalize(Vector3Add(Vector3Add(toCamera, Vector3Scale(right, -0.6f)), {0.0f, 0.8f, 0.0f}));
            SetShaderValue(shader, GetShaderLocation(shader, "uKey"), &key, SHADER_UNIFORM_VEC3);
            ::Model& native = *static_cast<::Model*>(model->native);
            for (int i = 0; i < native.meshCount; ++i) {
                ::Material& material = native.materials[native.meshMaterial[i]];
                const ::Shader previous = material.shader;
                material.shader = shader;
                DrawMesh(native.meshes[i], material,
                         MatrixMultiply(native.transform, MatrixTranslate(offset.x, offset.y, offset.z)));
                material.shader = previous;
            }
        }
        rlDrawRenderBatchActive();
        rlEnableBackfaceCulling();
        EndMode3D();
        EndTextureMode();

        // Render textures are stored upside down.
        ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)target_.texture.id, origin,
                                             ImVec2(origin.x + width, origin.y + height), ImVec2(0, 1), ImVec2(1, 0));
    }

    // Nothing to edit, so Save As is a copy of the file.
    bool SaveAs(const std::string& fullPath) override {
        std::error_code ec;
        return std::filesystem::copy_file(fullPath_, fullPath, ec) && !ec;
    }

   private:
    ServiceLocator& services_;
    std::string fullPath_;
    Path path_;
    ::RenderTexture2D target_{};
    float angle_ = 0.6f;
    bool spin_ = true;
};

}  // namespace

std::unique_ptr<ContentPane> MakeModelPane(ServiceLocator& services, const Services::EditorDocument& document) {
    return std::make_unique<ModelPane>(services, document);
}

}  // namespace Elysium
