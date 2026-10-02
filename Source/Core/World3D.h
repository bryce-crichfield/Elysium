#pragma once

#include <optional>
#include "Core/MathTypes.h"

namespace Elysium {

struct Model;
struct ModelComponent;
struct TransformComponent;

// The 3D world under the isometric picture. A Transform's x, y is a ground position as the
// World2D layers draw it: a 2:1 iso picture, seen by an orthographic camera tilted 30 degrees
// down. In 3D (GL axes: y up, z toward the camera) the ground point (x, y) at height z is
// (x, z, 2y): the camera shows ground depth at half (sin 30). Something h up is drawn
// h * cos 30 higher on screen. World3D layers draw through exactly this camera, so their
// models line up with the 2D layers pixel for pixel, and lighting's WorldTo3D agrees.
namespace World3D {

inline constexpr float kPitchSin = 0.5f;
inline constexpr float kPitchCos = 0.8660254f;
inline constexpr float kGroundDepth = 1.0f / kPitchSin;  // 3D depth per unit of 2D y

inline Vector3 ToGL(float x, float y, float z) { return {x, z, y * kGroundDepth}; }
// Where the 2D layers draw a 3D point (its 2D world position).
inline Vector2 To2D(Vector3 gl) { return {gl.x, gl.z * kPitchSin - gl.y * kPitchCos}; }

// The model's model-to-GL matrix (GL column-major, see RaylibConvert's ToRaylib).
Matrix ModelMatrix(const TransformComponent& transform, const ModelComponent& component, const Model& model);

// The model's GL-space bounding box (of its transformed model-space box).
void ModelBounds(const Matrix& modelMatrix, const Model& model, Vector3& min, Vector3& max);

// The 2D world rectangle the model covers on screen, for picking and selection.
Rectangle ModelBounds2D(const Matrix& modelMatrix, const Model& model);

// The highest surface of the model straight below `from` (GL), or nothing. Ray-casts every mesh.
std::optional<float> SurfaceBelow(const Matrix& modelMatrix, const Model& model, Vector3 from);

// Whether a ray from `from` (GL) along `direction` hits any of the model's meshes.
bool RayHits(const Matrix& modelMatrix, const Model& model, Vector3 from, Vector3 direction);

// How far along a ray from `from` (GL) along unit `direction` it first hits the model's
// meshes, or nothing.
std::optional<float> RayDistance(const Matrix& modelMatrix, const Model& model, Vector3 from, Vector3 direction);

// GL direction toward the camera (the view's depth axis).
inline const Vector3 kTowardCamera{0.0f, kPitchSin, kPitchCos};

// How far a model at the 2D world point `point` lies from the camera: the distance along the
// view ray through that point (from far toward the camera), or nothing if it misses. Larger
// is nearer the camera. Picking, which only knows a 2D point, uses it.
std::optional<float> PickDepth(const Matrix& modelMatrix, const Model& model, Vector2 point);

}  // namespace World3D
}  // namespace Elysium
