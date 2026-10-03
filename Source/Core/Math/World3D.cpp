#include "Core/Math/World3D.h"

#include <algorithm>
#include <cmath>

#include "Core/Components/ModelComponent.h"
#include "Core/Components/TransformComponent.h"
#include "Core/Graphics.h"
#include "Core/RaylibConvert.h"
#include "raylib.h"
#include "raymath.h"

namespace Elysium::World3D {

Matrix ModelMatrix(const TransformComponent& transform, const ModelComponent& component, const Model& model) {
    ::Matrix m = MatrixIdentity();
    if (component.centered) {
        // Footprint centered on the origin, base on the ground.
        m = MatrixTranslate(-(model.boundsMin[0] + model.boundsMax[0]) * 0.5f, -model.boundsMin[1],
                            -(model.boundsMin[2] + model.boundsMax[2]) * 0.5f);
    }
    const float scale = component.scale * 0.5f * (transform.worldScaleX + transform.worldScaleY);
    m = MatrixMultiply(m, MatrixScale(scale, scale, scale));
    // A 2D rotation turns clockwise on screen (y down); about GL y that's the negative angle.
    m = MatrixMultiply(m, MatrixRotateY(-(transform.worldRotation + component.yaw) * DEG2RAD));
    const Vector3 at = ToGL(transform.worldX, transform.worldY, transform.worldZ);
    m = MatrixMultiply(m, MatrixTranslate(at.x, at.y, at.z));
    return FromRaylib(m);
}

void ModelBounds(const Matrix& modelMatrix, const Model& model, Vector3& min, Vector3& max) {
    const ::Matrix m = ToRaylib(modelMatrix);
    min = {INFINITY, INFINITY, INFINITY};
    max = {-INFINITY, -INFINITY, -INFINITY};
    for (int corner = 0; corner < 8; ++corner) {
        const ::Vector3 local{(corner & 1) ? model.boundsMax[0] : model.boundsMin[0],
                              (corner & 2) ? model.boundsMax[1] : model.boundsMin[1],
                              (corner & 4) ? model.boundsMax[2] : model.boundsMin[2]};
        const ::Vector3 p = Vector3Transform(local, m);
        min = {std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z)};
        max = {std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z)};
    }
}

void ForEachTriangle(const Matrix& modelMatrix, const Model& model, const std::function<void(Vector3, Vector3, Vector3)>& visit) {
    if (!model.native) return;
    const ::Model& native = *static_cast<const ::Model*>(model.native);
    const ::Matrix m = ToRaylib(modelMatrix);
    for (int i = 0; i < native.meshCount; ++i) {
        const ::Mesh& mesh = native.meshes[i];
        if (!mesh.vertices) continue;
        auto vertex = [&](int index) {
            const ::Vector3 p = Vector3Transform({mesh.vertices[index * 3], mesh.vertices[index * 3 + 1], mesh.vertices[index * 3 + 2]}, m);
            return Vector3{p.x, p.y, p.z};
        };
        for (int t = 0; t < mesh.triangleCount; ++t) {
            int a = t * 3, b = t * 3 + 1, c = t * 3 + 2;
            if (mesh.indices) { a = mesh.indices[a]; b = mesh.indices[b]; c = mesh.indices[c]; }
            visit(vertex(a), vertex(b), vertex(c));
        }
    }
}

Rectangle ModelBounds2D(const Matrix& modelMatrix, const Model& model) {
    Vector3 min, max;
    ModelBounds(modelMatrix, model, min, max);
    // Screen y grows with depth (z) and shrinks with height (y).
    const Vector2 topLeft = To2D({min.x, max.y, min.z});
    const Vector2 bottomRight = To2D({max.x, min.y, max.z});
    return {topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y};
}

std::optional<float> SurfaceBelow(const Matrix& modelMatrix, const Model& model, Vector3 from) {
    if (!model.native) return std::nullopt;
    Vector3 min, max;
    ModelBounds(modelMatrix, model, min, max);
    if (from.x < min.x || from.x > max.x || from.z < min.z || from.z > max.z || from.y < min.y) return std::nullopt;

    const ::Model& native = *static_cast<const ::Model*>(model.native);
    const ::Matrix m = ToRaylib(modelMatrix);
    const ::Ray ray{{from.x, from.y, from.z}, {0.0f, -1.0f, 0.0f}};
    std::optional<float> best;
    for (int i = 0; i < native.meshCount; ++i) {
        const ::RayCollision hit = GetRayCollisionMesh(ray, native.meshes[i], m);
        if (hit.hit && (!best || hit.point.y > *best)) best = hit.point.y;
    }
    return best;
}

bool RayHits(const Matrix& modelMatrix, const Model& model, Vector3 from, Vector3 direction) {
    if (!model.native) return false;
    Vector3 min, max;
    ModelBounds(modelMatrix, model, min, max);
    const ::Ray ray{{from.x, from.y, from.z}, {direction.x, direction.y, direction.z}};
    if (!GetRayCollisionBox(ray, ::BoundingBox{{min.x, min.y, min.z}, {max.x, max.y, max.z}}).hit) return false;

    const ::Model& native = *static_cast<const ::Model*>(model.native);
    const ::Matrix m = ToRaylib(modelMatrix);
    for (int i = 0; i < native.meshCount; ++i) {
        if (GetRayCollisionMesh(ray, native.meshes[i], m).hit) return true;
    }
    return false;
}

std::optional<float> RayDistance(const Matrix& modelMatrix, const Model& model, Vector3 from, Vector3 direction) {
    if (!model.native) return std::nullopt;
    Vector3 min, max;
    ModelBounds(modelMatrix, model, min, max);
    const ::Ray ray{{from.x, from.y, from.z}, {direction.x, direction.y, direction.z}};
    if (!GetRayCollisionBox(ray, ::BoundingBox{{min.x, min.y, min.z}, {max.x, max.y, max.z}}).hit) return std::nullopt;

    const ::Model& native = *static_cast<const ::Model*>(model.native);
    const ::Matrix m = ToRaylib(modelMatrix);
    std::optional<float> nearest;
    for (int i = 0; i < native.meshCount; ++i) {
        const ::RayCollision hit = GetRayCollisionMesh(ray, native.meshes[i], m);
        if (hit.hit && (!nearest || hit.distance < *nearest)) nearest = hit.distance;
    }
    return nearest;
}

std::optional<float> PickDepth(const Matrix& modelMatrix, const Model& model, Vector2 point) {
    // The ground point under `point` at height 0 is on the view ray.
    constexpr float kFar = 100000.0f;
    const Vector3 ground = ToGL(point.x, point.y, 0.0f);
    // Cast from the camera's side so the first hit is the surface the cursor is over.
    const Vector3 eye{ground.x + kTowardCamera.x * kFar, ground.y + kTowardCamera.y * kFar,
                      ground.z + kTowardCamera.z * kFar};
    const auto distance = RayDistance(modelMatrix, model, eye, Vector3{-kTowardCamera.x, -kTowardCamera.y, -kTowardCamera.z});
    if (!distance) return std::nullopt;
    return kFar - *distance;
}

std::optional<float> PickDepth(const Matrix& modelMatrix, const Model& model, Vector3 ground, Vector3 toward) {
    constexpr float kFar = 100000.0f;
    const Vector3 eye{ground.x + toward.x * kFar, ground.y + toward.y * kFar, ground.z + toward.z * kFar};
    const auto distance = RayDistance(modelMatrix, model, eye, Vector3{-toward.x, -toward.y, -toward.z});
    if (!distance) return std::nullopt;
    return kFar - *distance;
}

namespace {

Vector3 Add(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 Sub(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vector3 Scale(Vector3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
Vector3 Normalize(Vector3 a) {
    const float length = std::sqrt(Dot(a, a));
    return length > 1e-12f ? Scale(a, 1.0f / length) : Vector3{0.0f, 0.0f, 1.0f};
}

constexpr float kOrthoBack = 50000.0f;   // how far back an orthographic view's rays start
constexpr float kOrthoDepth = 1.0e6f;    // an orthographic view's depth range, either way
constexpr float kFarGround = 50000.0f;   // where a ray that never meets the ground is cut

}  // namespace

// Screen right, screen up and back toward the camera, for an orbit `yaw` around the vertical
// and `pitch` down: turned, x' = cy x + sy z, z' = -sy x + cy z, then tilted.
static void Axes(float yaw, float pitch, Vector3& right, Vector3& up, Vector3& toward) {
    const float cy = cosf(yaw * DEG2RAD), sy = sinf(yaw * DEG2RAD);
    const float cp = cosf(pitch * DEG2RAD), sp = sinf(pitch * DEG2RAD);
    right = {cy, 0.0f, sy};
    up = {sp * sy, cp, -sp * cy};
    toward = {-cp * sy, sp, cp * cy};
}

void View::FillRows() {
    auto fill = [&](float* row, Vector3 axis, float scale, float offset) {
        row[0] = axis.x * scale; row[1] = axis.y * scale; row[2] = axis.z * scale;
        row[3] = offset - scale * Dot(axis, focus);
    };
    fill(rowX, right, zoom, width * 0.5f);
    fill(rowY, up, -zoom, height * 0.5f);
    fill(rowDepth, toward, zoom, 0.0f);
}

View::View(Vector2 focus2D, float zoom_, float width_, float height_, float yaw_, float pitch_) {
    perspective = false;
    width = width_; height = height_;
    zoom = zoom_; yaw = yaw_; pitch = pitch_;
    focus = ToGL(focus2D.x, focus2D.y, 0.0f);
    Axes(yaw, pitch, right, up, toward);
    eye = Add(focus, Scale(toward, kOrthoBack));
    FillRows();
    // Framebuffer pixels -> clip (y up), depth -> clip z (nearer is smaller).
    for (int c = 0; c < 4; ++c) {
        viewProjection.data[c * 4 + 0] = 2.0f / width * rowX[c] - (c == 3 ? 1.0f : 0.0f);
        viewProjection.data[c * 4 + 1] = -2.0f / height * rowY[c] + (c == 3 ? 1.0f : 0.0f);
        viewProjection.data[c * 4 + 2] = -rowDepth[c] / kOrthoDepth;
        viewProjection.data[c * 4 + 3] = c == 3 ? 1.0f : 0.0f;
    }
}

View View::Perspective(Vector2 focus2D, float distance, float fov, float width, float height, float yaw, float pitch) {
    View v;
    v.perspective = true;
    v.width = width; v.height = height;
    v.yaw = yaw; v.pitch = pitch;
    v.fov = std::clamp(fov, 5.0f, 150.0f);
    v.distance = std::max(distance, 1.0f);
    v.focus = ToGL(focus2D.x, focus2D.y, 0.0f);
    Axes(yaw, pitch, v.right, v.up, v.toward);
    v.eye = Add(v.focus, Scale(v.toward, v.distance));
    const float f = 1.0f / std::tan(v.fov * 0.5f * DEG2RAD);
    v.zoom = f * height * 0.5f / v.distance;
    v.FillRows();

    // View (camera space: x right, y up, z back) then a GL perspective projection.
    const float aspect = width / std::max(height, 1.0f);
    const float nearPlane = std::max(1.0f, v.distance * 0.01f);
    const float farPlane = v.distance * 20.0f + 20000.0f;
    const float a = (farPlane + nearPlane) / (nearPlane - farPlane);
    const float b = 2.0f * farPlane * nearPlane / (nearPlane - farPlane);
    const Vector3 axes[3] = {v.right, v.up, v.toward};
    float rows[3][4];
    for (int r = 0; r < 3; ++r) {
        rows[r][0] = axes[r].x; rows[r][1] = axes[r].y; rows[r][2] = axes[r].z;
        rows[r][3] = -Dot(axes[r], v.eye);
    }
    for (int c = 0; c < 4; ++c) {
        v.viewProjection.data[c * 4 + 0] = f / aspect * rows[0][c];
        v.viewProjection.data[c * 4 + 1] = f * rows[1][c];
        v.viewProjection.data[c * 4 + 2] = a * rows[2][c] + (c == 3 ? b : 0.0f);
        v.viewProjection.data[c * 4 + 3] = -rows[2][c];
    }
    return v;
}

Vector3 View::Project(Vector3 p) const {
    if (!perspective) {
        auto dot = [&](const float* r) { return r[0] * p.x + r[1] * p.y + r[2] * p.z + r[3]; };
        return {dot(rowX), dot(rowY), dot(rowDepth)};
    }
    const float* m = viewProjection.data;
    const float x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
    const float y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
    const float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
    if (w <= 1e-4f) return {-1.0e7f, -1.0e7f, -1.0e30f};
    return {(x / w + 1.0f) * 0.5f * width, (1.0f - y / w) * 0.5f * height, Dot(Sub(p, eye), toward)};
}

Vector2 View::WorldToFramebuffer(float x, float y, float z) const {
    const Vector3 p = Project(ToGL(x, y, z));
    return {p.x, p.y};
}

Ray View::RayAt(Vector2 fb) const {
    if (!perspective) {
        Vector3 origin = Add(focus, Scale(right, (fb.x - width * 0.5f) / zoom));
        origin = Add(origin, Scale(up, (height * 0.5f - fb.y) / zoom));
        return {Add(origin, Scale(toward, kOrthoBack)), Scale(toward, -1.0f)};
    }
    const float t = std::tan(fov * 0.5f * DEG2RAD);
    const float nx = fb.x / width * 2.0f - 1.0f, ny = 1.0f - fb.y / height * 2.0f;
    Vector3 direction = Scale(toward, -1.0f);
    direction = Add(direction, Scale(right, nx * t * width / std::max(height, 1.0f)));
    direction = Add(direction, Scale(up, ny * t));
    return {eye, Normalize(direction)};
}

Vector2 View::FramebufferToGround(Vector2 fb, float atHeight) const {
    const Ray ray = RayAt(fb);
    Vector3 hit = ray.At(kFarGround);
    if (ray.direction.y < -1e-6f) {
        const float t = (atHeight - ray.origin.y) / ray.direction.y;
        if (t > 0.0f && t < kFarGround) hit = ray.At(t);
    }
    return {hit.x, hit.z / kGroundDepth};
}

Vector3 View::TowardCamera(Vector3 at) const {
    return perspective ? Normalize(Sub(eye, at)) : toward;
}

float View::PixelsPerUnit(Vector3 at) const {
    if (!perspective) return zoom;
    const float depth = std::max(1.0f, Dot(Sub(eye, at), toward));
    return height * 0.5f / std::tan(fov * 0.5f * DEG2RAD) / depth;
}

Matrix View::GroundProjection() const {
    // Draw space (x, y, z) is GL (x, z, y * kGroundDepth): columns rearranged.
    Matrix g;
    const float* m = viewProjection.data;
    for (int r = 0; r < 4; ++r) {
        g.data[0 * 4 + r] = m[0 * 4 + r];
        g.data[1 * 4 + r] = m[2 * 4 + r] * kGroundDepth;
        g.data[2 * 4 + r] = m[1 * 4 + r];
        g.data[3 * 4 + r] = m[3 * 4 + r];
    }
    for (int c = 0; c < 4; ++c) g.data[c * 4 + 2] = 0.0f;  // no depth
    return g;
}

}  // namespace Elysium::World3D
