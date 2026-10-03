#include "Core/Assets/ModelAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_map>

namespace Elysium {

struct ModelAsset::Native {
    ::Model model{};
};

// A .mesh as the baker writes it (see bake_core.py): submeshes as index ranges over one
// shared vertex array.
struct ModelAsset::BakedMesh {
    struct Submesh {
        std::string name, material, texture;
        float color[4] = {1, 1, 1, 1};
        uint32_t firstIndex = 0, indexCount = 0;
    };
#pragma pack(push, 1)
    struct Vertex {
        float position[3];
        float normal[3];
        float uv[2];
        uint16_t joints[4];
        float weights[4];
    };
#pragma pack(pop)
    static_assert(sizeof(Vertex) == 56, "a .mesh vertex is 56 bytes");

    std::vector<Submesh> submeshes;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t boneCount = 0;
};

namespace {

bool IsBakedMesh(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return ext == ".mesh";
}

}  // namespace

ModelAsset::~ModelAsset() = default;

bool ModelAsset::Load() {
    const std::string path = GetPath().GetFullPath();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        LOG_ERRORF("ModelAsset", "Model file not found: %s", GetPath().c_str());
        return false;
    }
    if (!IsBakedMesh(path)) return true;  // raylib reads it in Finalize

    std::ifstream file(path, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    size_t at = 0;
    bool ok = true;
    auto raw = [&](void* out, size_t size) {
        if (!ok || at + size > bytes.size()) {
            ok = false;
            return;
        }
        std::memcpy(out, bytes.data() + at, size);
        at += size;
    };
    auto u32 = [&] { uint32_t v = 0; raw(&v, 4); return v; };
    auto text = [&] {
        uint16_t length = 0;
        raw(&length, 2);
        std::string s(length, '\0');
        if (length) raw(s.data(), length);
        return s;
    };

    char magic[4] = {};
    raw(magic, 4);
    if (!ok || std::memcmp(magic, "MESH", 4) != 0) {
        LOG_ERRORF("ModelAsset", "Not a .mesh file: %s", GetPath().c_str());
        return false;
    }
    if (const uint32_t version = u32(); version != 1) {
        LOG_ERRORF("ModelAsset", "Unsupported .mesh version %u: %s", version, GetPath().c_str());
        return false;
    }
    auto baked = std::make_unique<BakedMesh>();
    const uint32_t vertexCount = u32(), indexCount = u32(), submeshCount = u32();
    baked->boneCount = u32();
    baked->submeshes.resize(submeshCount);
    for (auto& sub : baked->submeshes) {
        sub.name = text();
        sub.material = text();
        raw(sub.color, sizeof(sub.color));
        sub.texture = text();
        sub.firstIndex = u32();
        sub.indexCount = u32();
    }
    if (ok) {
        baked->vertices.resize(vertexCount);
        raw(baked->vertices.data(), (size_t)vertexCount * sizeof(BakedMesh::Vertex));
        baked->indices.resize(indexCount);
        raw(baked->indices.data(), (size_t)indexCount * sizeof(uint32_t));
    }
    if (!ok) {
        LOG_ERRORF("ModelAsset", "Truncated .mesh file: %s", GetPath().c_str());
        return false;
    }
    for (uint32_t index : baked->indices) {
        if (index >= vertexCount) {
            LOG_ERRORF("ModelAsset", "Index out of range in %s", GetPath().c_str());
            return false;
        }
    }

    // The skeleton beside it, if it's skinned.
    if (baked->boneCount > 0) {
        const std::string skelPath = std::filesystem::path(path).replace_extension(".skel").string();
        if (std::filesystem::exists(skelPath, ec)) {
            auto skeleton = std::make_unique<Skeleton>();
            std::string error;
            if (!skeleton->Load(skelPath, error)) {
                LOG_ERRORF("ModelAsset", "Failed to load skeleton %s: %s", skelPath.c_str(), error.c_str());
            } else if (skeleton->bones.size() != baked->boneCount) {
                LOG_ERRORF("ModelAsset", "%s has %d bones but %s is skinned to %u; drawing it unskinned",
                           skelPath.c_str(), (int)skeleton->bones.size(), GetPath().c_str(), baked->boneCount);
            } else {
                skeleton_ = std::move(skeleton);
            }
        } else {
            LOG_WARNINGF("ModelAsset", "%s is skinned but has no .skel beside it; drawing it unskinned", GetPath().c_str());
        }
    }
    baked_ = std::move(baked);
    return true;
}

bool ModelAsset::Finalize() {
    ::Model model{};
    if (baked_) {
        // One raylib mesh per submesh, holding just the vertices it uses: raylib's indices
        // are 16 bit, so a submesh with more than 65535 of them is drawn unindexed.
        const BakedMesh& baked = *baked_;
        const bool skinned = skeleton_ != nullptr;
        const std::filesystem::path folder = std::filesystem::path(GetPath().GetFullPath()).parent_path();
        std::vector<const BakedMesh::Submesh*> subs;
        for (const auto& sub : baked.submeshes) {
            if (sub.indexCount >= 3 && (size_t)sub.firstIndex + sub.indexCount <= baked.indices.size()) subs.push_back(&sub);
        }
        if (subs.empty()) {
            LOG_ERRORF("ModelAsset", "No triangles in %s", GetPath().c_str());
            return false;
        }

        model.transform = MatrixIdentity();
        model.meshCount = model.materialCount = (int)subs.size();
        model.meshes = (::Mesh*)MemAlloc(sizeof(::Mesh) * subs.size());
        model.materials = (::Material*)MemAlloc(sizeof(::Material) * subs.size());
        model.meshMaterial = (int*)MemAlloc(sizeof(int) * subs.size());
        std::unordered_map<std::string, ::Texture2D> textures;
        bool clampedBones = false;

        for (size_t k = 0; k < subs.size(); ++k) {
            const BakedMesh::Submesh& sub = *subs[k];
            const uint32_t* first = baked.indices.data() + sub.firstIndex;
            std::unordered_map<uint32_t, uint32_t> remap;
            std::vector<uint32_t> used;  // baked vertex per mesh vertex
            for (uint32_t i = 0; i < sub.indexCount; ++i) {
                if (remap.try_emplace(first[i], (uint32_t)used.size()).second) used.push_back(first[i]);
            }
            const bool indexed = used.size() <= 65535;
            if (!indexed) {
                used.assign(first, first + sub.indexCount);
            }

            ::Mesh& mesh = model.meshes[k];
            mesh.vertexCount = (int)used.size();
            mesh.triangleCount = (int)(sub.indexCount / 3);
            mesh.vertices = (float*)MemAlloc(sizeof(float) * 3 * used.size());
            mesh.normals = (float*)MemAlloc(sizeof(float) * 3 * used.size());
            mesh.texcoords = (float*)MemAlloc(sizeof(float) * 2 * used.size());
            if (skinned) {
                mesh.boneIds = (unsigned char*)MemAlloc(4 * used.size());
                mesh.boneWeights = (float*)MemAlloc(sizeof(float) * 4 * used.size());
                mesh.boneCount = (int)skeleton_->bones.size();
            }
            for (size_t v = 0; v < used.size(); ++v) {
                const BakedMesh::Vertex& in = baked.vertices[used[v]];
                std::memcpy(mesh.vertices + v * 3, in.position, sizeof(in.position));
                std::memcpy(mesh.normals + v * 3, in.normal, sizeof(in.normal));
                std::memcpy(mesh.texcoords + v * 2, in.uv, sizeof(in.uv));
                if (skinned) {
                    for (int j = 0; j < 4; ++j) {
                        if (in.joints[j] > 255) clampedBones = true;
                        mesh.boneIds[v * 4 + j] = (unsigned char)std::min<uint16_t>(in.joints[j], 255);
                    }
                    std::memcpy(mesh.boneWeights + v * 4, in.weights, sizeof(in.weights));
                }
            }
            if (indexed) {
                mesh.indices = (unsigned short*)MemAlloc(sizeof(unsigned short) * sub.indexCount);
                for (uint32_t i = 0; i < sub.indexCount; ++i) mesh.indices[i] = (unsigned short)remap[first[i]];
            }
            UploadMesh(&mesh, false);

            ::Material& material = model.materials[k];
            material = LoadMaterialDefault();
            material.maps[MATERIAL_MAP_DIFFUSE].color = ::Color{
                (unsigned char)std::clamp(sub.color[0] * 255.0f, 0.0f, 255.0f),
                (unsigned char)std::clamp(sub.color[1] * 255.0f, 0.0f, 255.0f),
                (unsigned char)std::clamp(sub.color[2] * 255.0f, 0.0f, 255.0f),
                (unsigned char)std::clamp(sub.color[3] * 255.0f, 0.0f, 255.0f)};
            if (!sub.texture.empty()) {
                auto it = textures.find(sub.texture);
                if (it == textures.end()) {
                    const std::string file = (folder / sub.texture).string();
                    ::Texture2D texture{};
                    if (FileExists(file.c_str())) {
                        texture = LoadTexture(file.c_str());
                        GenTextureMipmaps(&texture);
                        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
                    } else {
                        LOG_WARNINGF("ModelAsset", "Texture %s (for %s) not found", file.c_str(), GetPath().c_str());
                    }
                    it = textures.emplace(sub.texture, texture).first;
                }
                if (it->second.id != 0) material.maps[MATERIAL_MAP_DIFFUSE].texture = it->second;
            }
            model.meshMaterial[k] = (int)k;
        }
        if (clampedBones) LOG_WARNINGF("ModelAsset", "%s uses bones past 255; they're clamped", GetPath().c_str());
        baked_.reset();
    } else {
        model = ::LoadModel(GetPath().c_str());
        if (model.meshCount == 0) {
            LOG_ERRORF("ModelAsset", "Failed to load model: %s", GetPath().c_str());
            ::UnloadModel(model);
            return false;
        }
    }

    native_ = std::make_unique<Native>();
    native_->model = model;
    const ::BoundingBox box = ::GetModelBoundingBox(model);
    model_ = Model{};
    model_.meshCount = model.meshCount;
    model_.materialCount = model.materialCount;
    model_.boundsMin[0] = box.min.x; model_.boundsMin[1] = box.min.y; model_.boundsMin[2] = box.min.z;
    model_.boundsMax[0] = box.max.x; model_.boundsMax[1] = box.max.y; model_.boundsMax[2] = box.max.z;
    model_.native = &native_->model;
    model_.skeleton = skeleton_.get();
    SetLoaded(true);
    LOG_DEBUGF("ModelAsset", "Model loaded: %s (%d meshes, %.2f x %.2f x %.2f%s)", GetPath().c_str(), model.meshCount,
               box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z,
               skeleton_ ? ", skinned" : "");
    return true;
}

void ModelAsset::Unload() {
    if (native_) {
        // Submeshes may share a texture: unload each one once, then the rest of the model.
        ::Model& model = native_->model;
        std::vector<unsigned int> seen;
        for (int i = 0; i < model.materialCount; ++i) {
            ::Texture2D& texture = model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
            if (texture.id == rlGetTextureIdDefault()) continue;
            if (std::find(seen.begin(), seen.end(), texture.id) != seen.end()) {
                texture.id = rlGetTextureIdDefault();  // UnloadMaterial skips the default texture
            } else {
                seen.push_back(texture.id);
            }
        }
        ::UnloadModel(model);
    }
    native_.reset();
    baked_.reset();
    skeleton_.reset();
    model_ = Model{};
    SetLoaded(false);
}

REGISTER_ASSET_TYPE(Model, ModelAsset);

}  // namespace Elysium
