#include "Core/Animation.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

namespace Elysium {

namespace {

// Little-endian reads over a whole file in memory; any read past the end fails the reader.
struct Reader {
    std::vector<char> bytes;
    size_t at = 0;
    bool ok = true;

    bool Open(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        return true;
    }
    void Raw(void* out, size_t size) {
        if (!ok || at + size > bytes.size()) {
            ok = false;
            std::memset(out, 0, size);
            return;
        }
        std::memcpy(out, bytes.data() + at, size);
        at += size;
    }
    template <typename T>
    T Get() {
        T value{};
        Raw(&value, sizeof(T));
        return value;
    }
    void Floats(float* out, size_t count) { Raw(out, count * sizeof(float)); }
    std::string String() {
        const uint16_t length = Get<uint16_t>();
        std::string text(length, '\0');
        if (length) Raw(text.data(), length);
        return text;
    }
    bool Magic(const char* tag) {
        char got[4];
        Raw(got, 4);
        return ok && std::memcmp(got, tag, 4) == 0;
    }
};

// Column-major 4x4: element (row r, column c) is m[c * 4 + r].
Matrix Multiply(const Matrix& a, const Matrix& b) {
    Matrix out;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.data[k * 4 + r] * b.data[c * 4 + k];
            out.data[c * 4 + r] = sum;
        }
    }
    return out;
}

Matrix Compose(const BoneTransform& b) {
    const float x = b.q[0], y = b.q[1], z = b.q[2], w = b.q[3];
    Matrix m;
    m.data[0] = (1 - 2 * (y * y + z * z)) * b.s[0];
    m.data[1] = (2 * (x * y + z * w)) * b.s[0];
    m.data[2] = (2 * (x * z - y * w)) * b.s[0];
    m.data[4] = (2 * (x * y - z * w)) * b.s[1];
    m.data[5] = (1 - 2 * (x * x + z * z)) * b.s[1];
    m.data[6] = (2 * (y * z + x * w)) * b.s[1];
    m.data[8] = (2 * (x * z + y * w)) * b.s[2];
    m.data[9] = (2 * (y * z - x * w)) * b.s[2];
    m.data[10] = (1 - 2 * (x * x + y * y)) * b.s[2];
    m.data[12] = b.t[0];
    m.data[13] = b.t[1];
    m.data[14] = b.t[2];
    return m;
}

void Lerp3(float* out, const float* a, const float* b, float k) {
    for (int i = 0; i < 3; ++i) out[i] = a[i] + (b[i] - a[i]) * k;
}

// Normalized lerp along the shorter arc: close enough to slerp between neighbouring frames
// and crossfades, and cheaper.
void Nlerp(float* out, const float* a, const float* b, float k) {
    const float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    const float sign = dot < 0.0f ? -1.0f : 1.0f;
    float length = 0.0f;
    for (int i = 0; i < 4; ++i) {
        out[i] = a[i] + (b[i] * sign - a[i]) * k;
        length += out[i] * out[i];
    }
    length = std::sqrt(length);
    if (length < 1e-8f) {
        std::memcpy(out, a, sizeof(float) * 4);
        return;
    }
    for (int i = 0; i < 4; ++i) out[i] /= length;
}

}  // namespace

int Skeleton::Find(const std::string& name) const {
    for (size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].name == name) return (int)i;
    }
    return -1;
}

bool Skeleton::Load(const std::string& path, std::string& error) {
    Reader in;
    if (!in.Open(path)) {
        error = "can't open";
        return false;
    }
    if (!in.Magic("SKEL")) {
        error = "not a .skel file";
        return false;
    }
    if (const uint32_t version = in.Get<uint32_t>(); version != 1) {
        error = "unsupported version " + std::to_string(version);
        return false;
    }
    const uint32_t count = in.Get<uint32_t>();
    bones.clear();
    bones.resize(count);
    for (uint32_t i = 0; i < count && in.ok; ++i) {
        Bone& bone = bones[i];
        bone.name = in.String();
        bone.parent = in.Get<int32_t>();
        in.Floats(bone.bind.t, 3);
        in.Floats(bone.bind.q, 4);
        in.Floats(bone.bind.s, 3);
        in.Floats(bone.inverseBind.data, 16);
        if (bone.parent >= (int)i) {
            error = "bone '" + bone.name + "' comes before its parent";
            return false;
        }
    }
    if (!in.ok) {
        error = "file is truncated";
        return false;
    }
    return true;
}

bool Animation::Load(const std::string& path, std::string& error) {
    Reader in;
    if (!in.Open(path)) {
        error = "can't open";
        return false;
    }
    if (!in.Magic("ANIM")) {
        error = "not a .anim file";
        return false;
    }
    if (const uint32_t version = in.Get<uint32_t>(); version != 1) {
        error = "unsupported version " + std::to_string(version);
        return false;
    }
    fps = in.Get<float>();
    duration = in.Get<float>();
    frameCount = (int)in.Get<uint32_t>();
    const uint32_t trackCount = in.Get<uint32_t>();
    if (!in.ok || frameCount <= 0 || fps <= 0.0f) {
        error = "bad header";
        return false;
    }
    tracks.clear();
    tracks.resize(trackCount);
    for (uint32_t i = 0; i < trackCount && in.ok; ++i) {
        Track& track = tracks[i];
        track.bone = in.String();
        const uint8_t flags = in.Get<uint8_t>();
        auto channel = [&](std::vector<float>& out, int width, int bit) {
            out.resize((size_t)width * ((flags >> bit) & 1 ? 1 : frameCount));
            in.Floats(out.data(), out.size());
        };
        channel(track.t, 3, 0);
        channel(track.r, 4, 1);
        channel(track.s, 3, 2);
    }
    if (!in.ok) {
        error = "file is truncated";
        return false;
    }
    return true;
}

namespace Skinning {

std::vector<int> BindTracks(const Animation& clip, const Skeleton& skeleton) {
    std::vector<int> bones(clip.tracks.size(), -1);
    for (size_t i = 0; i < clip.tracks.size(); ++i) bones[i] = skeleton.Find(clip.tracks[i].bone);
    return bones;
}

void Sample(const Animation& clip, const std::vector<int>& trackBones, float time, std::vector<BoneTransform>& pose) {
    const float frame = std::clamp(time, 0.0f, clip.duration) * clip.fps;
    const int f0 = std::clamp((int)std::floor(frame), 0, clip.frameCount - 1);
    const int f1 = std::min(f0 + 1, clip.frameCount - 1);
    const float k = std::clamp(frame - (float)f0, 0.0f, 1.0f);
    for (size_t i = 0; i < clip.tracks.size() && i < trackBones.size(); ++i) {
        const int bone = trackBones[i];
        if (bone < 0 || bone >= (int)pose.size()) continue;
        const Animation::Track& track = clip.tracks[i];
        BoneTransform& out = pose[bone];
        // A constant channel holds one value: both frames are it.
        auto at = [](const std::vector<float>& values, int width, int f) {
            return values.data() + (values.size() == (size_t)width ? 0 : (size_t)f * width);
        };
        Lerp3(out.t, at(track.t, 3, f0), at(track.t, 3, f1), k);
        Nlerp(out.q, at(track.r, 4, f0), at(track.r, 4, f1), k);
        Lerp3(out.s, at(track.s, 3, f0), at(track.s, 3, f1), k);
    }
}

void Blend(std::vector<BoneTransform>& pose, const std::vector<BoneTransform>& target, float weight) {
    const size_t count = std::min(pose.size(), target.size());
    for (size_t i = 0; i < count; ++i) {
        BoneTransform from = pose[i];
        Lerp3(pose[i].t, from.t, target[i].t, weight);
        Nlerp(pose[i].q, from.q, target[i].q, weight);
        Lerp3(pose[i].s, from.s, target[i].s, weight);
    }
}

void SkinMatrices(const Skeleton& skeleton, const std::vector<BoneTransform>& pose, std::vector<Matrix>& skin) {
    const size_t count = skeleton.bones.size();
    std::vector<Matrix> world(count);
    skin.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const Bone& bone = skeleton.bones[i];
        const Matrix local = Compose(i < pose.size() ? pose[i] : bone.bind);
        world[i] = bone.parent < 0 ? local : Multiply(world[bone.parent], local);
        skin[i] = Multiply(world[i], bone.inverseBind);
    }
}

}  // namespace Skinning

}  // namespace Elysium
