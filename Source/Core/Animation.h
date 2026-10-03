#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Core/Math/MathTypes.h"

namespace Elysium {

// Skeletal animation data, as the baker (Tools: bake_core.py) writes it: a .skel next to a
// .mesh, and .anim clips keyed by bone name. All of it is in the model's own space: right
// handed, y up, glTF units. Quaternions are (x, y, z, w); matrices column-major.

// A bone's local transform, relative to its parent.
struct BoneTransform {
    float t[3] = {0.0f, 0.0f, 0.0f};
    float q[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float s[3] = {1.0f, 1.0f, 1.0f};
};

struct Bone {
    std::string name;
    int parent = -1;          // always before this bone in the list; -1 for a root
    BoneTransform bind;       // the rest pose
    Matrix inverseBind;       // skin matrix = bone's model-space matrix * inverseBind
};

// The bones a .mesh is skinned to (a .skel file). Parents come before their children, so a
// pose resolves in one pass.
struct Skeleton {
    std::vector<Bone> bones;

    int Find(const std::string& name) const;
    // Reads a .skel file; false (with `error` set) if it isn't one.
    bool Load(const std::string& path, std::string& error);
};

// One clip (a .anim file): every frame of every animated bone, sampled at `fps`. Frame i is
// at i / fps; the last frame is at `duration`. A channel that never changes holds one value.
struct Animation {
    struct Track {
        std::string bone;
        std::vector<float> t;  // 3 per frame, or 3 if constant
        std::vector<float> r;  // 4 per frame, or 4
        std::vector<float> s;  // 3 per frame, or 3
    };

    float fps = 30.0f;
    float duration = 0.0f;
    int frameCount = 0;
    std::vector<Track> tracks;

    bool Load(const std::string& path, std::string& error);
};

namespace Skinning {

// Each track's bone in `skeleton` (-1 where the skeleton has no bone by that name).
std::vector<int> BindTracks(const Animation& clip, const Skeleton& skeleton);

// The skeleton's local pose at `time` seconds into `clip` (clamped to it): the bind pose,
// with every bound track's bone replaced by the clip's frames, interpolated.
void Sample(const Animation& clip, const std::vector<int>& trackBones, float time, std::vector<BoneTransform>& pose);

// `pose` moved `weight` (0..1) of the way toward `target`.
void Blend(std::vector<BoneTransform>& pose, const std::vector<BoneTransform>& target, float weight);

// The skin matrices of a local pose: each bone's model-space matrix times its inverse bind.
void SkinMatrices(const Skeleton& skeleton, const std::vector<BoneTransform>& pose, std::vector<Matrix>& skin);

}  // namespace Skinning

}  // namespace Elysium
