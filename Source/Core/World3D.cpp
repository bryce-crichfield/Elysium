#include "Core/World3D.h"

#include <algorithm>
#include <cmath>

#include "Components/ModelComponent.h"
#include "Components/TransformComponent.h"
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

View::View(Vector2 focus, float zoom_, float width, float height, float yaw, float pitch_) : zoom(zoom_), pitch(pitch_) {
    // Turn the world about the vertical through the focus, tilt it toward the camera, scale.
    const float cy = cosf(yaw * DEG2RAD), sy = sinf(yaw * DEG2RAD);
    const float cp = cosf(pitch * DEG2RAD), sp = sinf(pitch * DEG2RAD);
    const Vector3 f = ToGL(focus.x, focus.y, 0.0f);
    // Turned: x' = cy x + sy z, z' = -sy x + cy z. Screen x is x', screen y is z' sin p less
    // the height cos p, depth is the height sin p plus z' cos p.
    const float x[3] = {zoom * cy, 0.0f, zoom * sy};
    const float y[3] = {-zoom * sp * sy, -zoom * cp, zoom * sp * cy};
    const float d[3] = {-zoom * cp * sy, zoom * sp, zoom * cp * cy};
    auto fill = [&](float* row, const float* c, float offset) {
        row[0] = c[0]; row[1] = c[1]; row[2] = c[2];
        row[3] = offset - (c[0] * f.x + c[1] * f.y + c[2] * f.z);
    };
    fill(rowX, x, width * 0.5f);
    fill(rowY, y, height * 0.5f);
    fill(rowDepth, d, 0.0f);
}

Vector3 View::Project(Vector3 p) const {
    auto dot = [&](const float* r) { return r[0] * p.x + r[1] * p.y + r[2] * p.z + r[3]; };
    return {dot(rowX), dot(rowY), dot(rowDepth)};
}

Vector2 View::WorldToFramebuffer(float x, float y, float z) const {
    const Vector3 p = Project(ToGL(x, y, z));
    return {p.x, p.y};
}

Vector2 View::FramebufferToGround(Vector2 fb) const {
    // On the ground GL y is 0 and z is 2 * ground y: solve the 2x2 of (x, z).
    const float a = rowX[0], b = rowX[2], c = rowY[0], d = rowY[2];
    const float det = a * d - b * c;
    if (std::fabs(det) < 1e-12f) return {0.0f, 0.0f};
    const float u = fb.x - rowX[3], v = fb.y - rowY[3];
    const float gx = (d * u - b * v) / det, gz = (a * v - c * u) / det;
    return {gx, gz / kGroundDepth};
}

}  // namespace Elysium::World3D
