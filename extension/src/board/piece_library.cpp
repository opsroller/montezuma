// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "piece_library.hpp"

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/prism_mesh.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace montezuma::godot_binding {

namespace {

// Kind order matches the LevelData contract: red, orange, yellow, green,
// blue, purple, white. Each placeholder also has its own silhouette, so
// kinds never differ by colour alone.
const Color kKindColors[PieceLibrary::kMaxKinds] = {
    Color(0.78f, 0.10f, 0.08f), Color(0.95f, 0.42f, 0.04f), Color(0.95f, 0.78f, 0.18f), Color(0.15f, 0.60f, 0.22f),
    Color(0.12f, 0.35f, 0.85f), Color(0.50f, 0.20f, 0.68f), Color(0.90f, 0.88f, 0.82f),
};

// Placeholders are told apart from above by outline as well as colour:
// circle, diamond, octagon coin, triangle, ring, pill, hexagon.
Ref<Mesh> placeholder_mesh(int kind, real_t s) {
    switch (kind) {
        case 0: {
            Ref<SphereMesh> m;
            m.instantiate();
            m->set_radius(s * 0.5f);
            m->set_height(s);
            m->set_radial_segments(24);
            m->set_rings(12);
            return m;
        }
        case 1: {
            Ref<BoxMesh> m;
            m.instantiate();
            m->set_size(Vector3(s * 0.72f, s * 0.6f, s * 0.72f));
            return m;
        }
        case 2: {
            Ref<CylinderMesh> m;
            m.instantiate();
            m->set_top_radius(s * 0.48f);
            m->set_bottom_radius(s * 0.48f);
            m->set_height(s * 0.35f);
            m->set_radial_segments(8);
            return m;
        }
        case 3: {
            Ref<PrismMesh> m;
            m.instantiate();
            m->set_size(Vector3(s, s * 0.9f, s * 0.55f));
            return m;
        }
        case 4: {
            Ref<TorusMesh> m;
            m.instantiate();
            m->set_inner_radius(s * 0.2f);
            m->set_outer_radius(s * 0.48f);
            return m;
        }
        case 5: {
            Ref<CapsuleMesh> m;
            m.instantiate();
            m->set_radius(s * 0.26f);
            m->set_height(s * 1.0f);
            return m;
        }
        default: {
            Ref<CylinderMesh> m;
            m.instantiate();
            m->set_top_radius(s * 0.3f);
            m->set_bottom_radius(s * 0.48f);
            m->set_height(s * 0.6f);
            m->set_radial_segments(6);
            return m;
        }
    }
}

// Half height of each placeholder as it stands, in units of the piece size,
// so it can rest on the board instead of floating at the cell centre.
constexpr real_t kHalfHeight[PieceLibrary::kMaxKinds] = {0.5f, 0.3f, 0.175f, 0.275f, 0.14f, 0.26f, 0.3f};

// Turns each placeholder so its outline faces the camera.
Basis placeholder_basis(int kind) {
    switch (kind) {
        case 1: return Basis(Vector3(0, 1, 0), static_cast<real_t>(Math_PI / 4.0));   // square -> diamond
        case 3: return Basis(Vector3(1, 0, 0), static_cast<real_t>(-Math_PI / 2.0));  // triangle face up
        case 5: return Basis(Vector3(0, 0, 1), static_cast<real_t>(Math_PI / 2.0));   // pill lies flat
        default: return Basis();
    }
}

}  // namespace

void PieceLibrary::load(double cell, const String& theme) {
    entries_.clear();
    if (FileAccess::file_exists(kManifestPath)) {
        const Variant parsed = JSON::parse_string(FileAccess::get_file_as_string(kManifestPath));
        if (parsed.get_type() == Variant::DICTIONARY) {
            const Dictionary manifest = parsed;
            const String name = theme.is_empty() ? String(manifest.get("default_theme", "")) : theme;
            const Variant themes = manifest.get("themes", Variant());
            const Variant entries =
                themes.get_type() == Variant::DICTIONARY ? Dictionary(themes).get(name, Variant()) : Variant();
            if (entries.get_type() == Variant::DICTIONARY) {
                entries_ = entries;
            } else {
                UtilityFunctions::push_warning("Asset manifest has no theme '", name, "'; using placeholders.");
            }
        } else {
            UtilityFunctions::push_warning("Asset manifest is not a JSON object; using placeholders: ", kManifestPath);
        }
    }
    match_fx_ = load_entry("fx_match");
    const real_t s = static_cast<real_t>(cell) * 0.7f;
    piece_size_ = s;
    for (int k = 0; k < kMaxKinds; ++k) {
        scenes_[k].unref();
        meshes_[k].unref();
        materials_[k].unref();
        const Ref<Resource> res = load_entry(String("artifact_") + String::num_int64(k));
        if (Ref<PackedScene> scene = res; scene.is_valid()) {
            scenes_[k] = scene;
            continue;
        }
        if (Ref<Mesh> mesh = res; mesh.is_valid()) {
            meshes_[k] = mesh;  // the mesh brings its own materials
            continue;
        }
        meshes_[k] = placeholder_mesh(k, s);
        Ref<StandardMaterial3D> mat;
        mat.instantiate();
        mat->set_albedo(kKindColors[k]);
        mat->set_roughness(0.45f);
        mat->set_metallic(k == 2 ? 0.8f : 0.1f);  // gold reads as gold
        materials_[k] = mat;
    }
}

Ref<Resource> PieceLibrary::load_entry(const String& name) const {
    if (!entries_.has(name)) return {};
    const String path = entries_[name];
    if (path.is_empty() || !ResourceLoader::get_singleton()->exists(path)) {
        UtilityFunctions::push_warning("Asset manifest entry '", name, "' does not load (", path,
                                       "); using a placeholder.");
        return {};
    }
    return ResourceLoader::get_singleton()->load(path);
}

Node3D* PieceLibrary::make_visual(int kind) const {
    if (kind < 0 || kind >= kMaxKinds) return memnew(Node3D);
    if (scenes_[kind].is_valid()) {
        if (auto* node = Object::cast_to<Node3D>(scenes_[kind]->instantiate())) return node;
    }
    // The mesh sits under a holder node: the board view moves and scales the
    // holder, and the mesh keeps its own orientation.
    auto* holder = memnew(Node3D);
    auto* mi = memnew(MeshInstance3D);
    mi->set_mesh(meshes_[kind]);
    if (materials_[kind].is_valid()) {
        mi->set_material_override(materials_[kind]);
        mi->set_basis(placeholder_basis(kind));
        mi->set_position(Vector3(0, (kHalfHeight[kind] - 0.5f) * piece_size_, 0));
    }
    holder->add_child(mi);
    return holder;
}

Node3D* PieceLibrary::acquire(int kind) {
    if (kind >= 0 && kind < kMaxKinds && !pool_[kind].empty()) {
        Node3D* node = pool_[kind].back();
        pool_[kind].pop_back();
        return node;
    }
    return make_visual(kind);
}

void PieceLibrary::release(int kind, Node3D* node) {
    if (!node) return;
    if (kind < 0 || kind >= kMaxKinds) {
        memdelete(node);
        return;
    }
    node->set_position(Vector3());
    node->set_scale(Vector3(1, 1, 1));
    node->set_rotation(Vector3());
    node->set_visible(true);
    pool_[kind].push_back(node);
}

void PieceLibrary::clear() {
    for (auto& nodes : pool_) {
        for (Node3D* node : nodes) memdelete(node);
        nodes.clear();
    }
}

Node3D* PieceLibrary::instantiate_entry(const String& name) const {
    const Ref<PackedScene> scene = load_entry(name);
    if (scene.is_null()) return nullptr;
    Node* node = scene->instantiate();
    if (auto* node3d = Object::cast_to<Node3D>(node)) return node3d;
    UtilityFunctions::push_warning("Asset manifest entry '", name, "' is not a Node3D scene; using a placeholder.");
    if (node) memdelete(node);
    return nullptr;
}

Node3D* PieceLibrary::make_highlight() const { return instantiate_entry("cell_highlight"); }

Node3D* PieceLibrary::make_match_fx() const {
    if (match_fx_.is_null()) return nullptr;
    Node* node = match_fx_->instantiate();
    auto* fx = Object::cast_to<CPUParticles3D>(node);
    if (!fx) {
        if (node) memdelete(node);
        return nullptr;
    }
    fx->set_one_shot(true);
    fx->set_emitting(true);
    fx->connect("finished", Callable(fx, "queue_free"), Object::CONNECT_ONE_SHOT);
    return fx;
}

Node3D* PieceLibrary::make_environment() const {
    if (Node3D* node = instantiate_entry("environment")) return node;
    // The look the board scene had before themes: a soft sky, a warm key
    // light with shadows and a cool fill. Basis values are rows, as in a .tscn.
    Ref<ProceduralSkyMaterial> sky_mat;
    sky_mat.instantiate();
    sky_mat->set_sky_top_color(Color(0.2f, 0.42f, 0.55f));
    sky_mat->set_sky_horizon_color(Color(0.62f, 0.7f, 0.62f));
    sky_mat->set_ground_bottom_color(Color(0.16f, 0.22f, 0.12f));
    sky_mat->set_ground_horizon_color(Color(0.62f, 0.7f, 0.62f));
    Ref<Sky> sky;
    sky.instantiate();
    sky->set_material(sky_mat);
    Ref<Environment> env;
    env.instantiate();
    env->set_background(Environment::BG_SKY);
    env->set_sky(sky);
    env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
    env->set_ambient_light_energy(0.45f);
    env->set_reflection_source(Environment::REFLECTION_SOURCE_SKY);
    env->set_tonemapper(Environment::TONE_MAPPER_ACES);

    auto* root = memnew(Node3D);
    auto* world = memnew(WorldEnvironment);
    world->set_name("WorldEnvironment");
    world->set_environment(env);
    root->add_child(world);

    auto* key = memnew(DirectionalLight3D);
    key->set_name("KeyLight");
    key->set_transform(Transform3D(Basis(0.906308f, -0.383022f, 0.178606f, 0, 0.422618f, 0.906308f, -0.422618f,
                                         -0.821394f, 0.383022f),
                                   Vector3(0, 6, 0)));
    key->set_color(Color(1, 0.95f, 0.85f));
    key->set_param(Light3D::PARAM_ENERGY, 0.9f);
    key->set_shadow(true);
    key->set_param(Light3D::PARAM_SHADOW_OPACITY, 0.6f);
    key->set_shadow_mode(DirectionalLight3D::SHADOW_ORTHOGONAL);
    key->set_param(Light3D::PARAM_SHADOW_MAX_DISTANCE, 40.0f);
    root->add_child(key);

    auto* fill = memnew(DirectionalLight3D);
    fill->set_name("FillLight");
    fill->set_transform(Transform3D(Basis(-0.866025f, 0.25f, -0.433013f, 0, 0.866025f, 0.5f, 0.5f, 0.433013f, -0.75f),
                                    Vector3(0, 6, 0)));
    fill->set_color(Color(0.75f, 0.85f, 1));
    fill->set_param(Light3D::PARAM_ENERGY, 0.35f);
    root->add_child(fill);
    return root;
}

Node3D* PieceLibrary::make_board(int width, int height, double cell) const {
    if (Node3D* node = instantiate_entry("board")) return node;
    const real_t c = static_cast<real_t>(cell);
    Ref<BoxMesh> slab;
    slab.instantiate();
    slab->set_size(Vector3(static_cast<real_t>(width) * c + 0.4f * c, 0.3f * c, static_cast<real_t>(height) * c + 0.4f * c));
    Ref<StandardMaterial3D> stone;
    stone.instantiate();
    stone->set_albedo(Color(0.30f, 0.27f, 0.23f));
    stone->set_roughness(0.95f);
    auto* node = memnew(MeshInstance3D);
    node->set_mesh(slab);
    node->set_material_override(stone);
    node->set_position(Vector3(0, -0.5f * c, 0));

    // Alternating cell tiles on top of the slab, so rows and columns read.
    Ref<Image> checker = Image::create_empty(width, height, false, Image::FORMAT_RGBA8);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            checker->set_pixel(x, y, (x + y) % 2 == 0 ? Color(0.36f, 0.32f, 0.27f) : Color(0.28f, 0.25f, 0.21f));
        }
    }
    Ref<StandardMaterial3D> tiles;
    tiles.instantiate();
    tiles->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, ImageTexture::create_from_image(checker));
    tiles->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
    tiles->set_roughness(0.9f);
    Ref<PlaneMesh> plane;
    plane.instantiate();
    plane->set_size(Vector2(static_cast<real_t>(width) * c, static_cast<real_t>(height) * c));
    auto* inlay = memnew(MeshInstance3D);
    inlay->set_name("Tiles");
    inlay->set_mesh(plane);
    inlay->set_material_override(tiles);
    inlay->set_position(Vector3(0, 0.151f * c, 0));  // just above the slab's top face
    node->add_child(inlay);
    return node;
}

}  // namespace montezuma::godot_binding
